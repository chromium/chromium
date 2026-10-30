#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages Android Lint as lint.jar plus a GraalVM native image.

The native image (bin/lint-graalvm) avoids JVM warm-up in the hundreds of
per-library lint analyses. Native images cannot load classes at run time, so
the lint checks that androidx aars ship (lint.jar inside the .aar) are
compiled in. The CRC-32 of each of those lint.jar files is part of the package
version, so a new package is built when lint or any of those checks change, but
not for androidx rolls that leave them alone.
"""

import glob
import os
import pathlib
import re
import shutil
import sys
import zipfile

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(p for p in _THIS_DIR.parents
                 if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common
import graalvm
import maven

_MAIN_CLASS = 'com.android.tools.lint.Main'
_ANDROIDX_LIBS = 'third_party/androidx/cipd/libs'
_SDK_DIR = 'third_party/android_sdk/public'
_KOTLIN_STDLIB = ('third_party/android_deps/cipd/libs/'
                  'org_jetbrains_kotlin_kotlin_stdlib/kotlin-stdlib.jar')
# Aars that are not lint checks but that the sample sources reference, so that
# checks that resolve their types run while tracing.
_SAMPLE_CLASSPATH_LIBS = (
    'androidx_annotation_annotation_jvm',
    'androidx_core_core',
    'androidx_lifecycle_lifecycle_common_jvm',
    'androidx_lifecycle_lifecycle_livedata',
)


def _aars_with_lint_jars(src_root):
    """Returns (aar, lint.jar CRC-32) for each androidx .aar with lint checks."""
    ret = []
    for aar in sorted(glob.glob(os.path.join(src_root, _ANDROIDX_LIBS, '*',
                                             '*.aar'))):
        with zipfile.ZipFile(aar) as z:
            if 'lint.jar' in z.namelist():
                ret.append((aar, z.getinfo('lint.jar').CRC))
    # The aars come from DEPS, which the 3pp packager syncs only with
    # gclient_apply_config = ["android"]. Without them, the image would lack
    # androidx checks and the version would not track them.
    if not ret:
        raise RuntimeError('No .aar with lint.jar under //' + _ANDROIDX_LIBS)
    return ret


def _sample_classpath_files(src_root):
    ret = []
    for lib in _SAMPLE_CLASSPATH_LIBS:
        ret += sorted(
            glob.glob(os.path.join(src_root, _ANDROIDX_LIBS, lib, '*.[aj]ar')))
    return ret


def _sdk_platform(src_root):
    # The sample needs android.jar to resolve framework APIs, as in the build.
    platforms = sorted(glob.glob(os.path.join(src_root, _SDK_DIR, 'platforms',
                                              'android-*')))
    return platforms[-1]


def _version_strings():
    # The image compiles in the aar lint checks, so it must be rebuilt when they
    # change. androidx rolls change every aar's bytes but rarely the lint.jar
    # inside, whose entries androidx writes with fixed timestamps, so its CRC-32
    # changes only with its contents.
    ret = graalvm.version_strings()
    for aar, crc in _aars_with_lint_jars(str(_SRC_ROOT)):
        ret.append('{}={:08x}'.format(os.path.basename(os.path.dirname(aar)),
                                      crc))
    return ret


def _rel(path):
    return '//' + os.path.relpath(path, str(_SRC_ROOT))


def _runtime_deps():
    deps = [_rel(aar) for aar, _ in _aars_with_lint_jars(str(_SRC_ROOT))]
    deps += [_rel(p) for p in _sample_classpath_files(str(_SRC_ROOT))]
    deps.append(_rel(_sdk_platform(str(_SRC_ROOT))))
    deps.append('//' + _KOTLIN_STDLIB)
    return deps


def _extract_aars(aars, dest_dir):
    """Unpacks aars the way the build does for lint (<aar extracted=...>)."""
    ret = []
    for aar in aars:
        out = os.path.join(dest_dir, os.path.basename(os.path.dirname(aar)))
        with zipfile.ZipFile(aar) as z:
            z.extractall(out)
        ret.append((aar, out))
    return ret


def _check_lint_jar_conflicts(lint_jars):
    """Fails if two aar lint jars ship different versions of one class.

    On the JVM, lint loads each aar's lint.jar in its own class loader. The
    image has a single class path, where such a class would resolve to one of
    the versions for every check that uses it.
    """
    seen = {}
    for jar in lint_jars:
        with zipfile.ZipFile(jar) as z:
            for info in z.infolist():
                name = info.filename
                if not name.endswith('.class'):
                    continue
                digest = info.CRC
                if seen.setdefault(name, (digest, jar))[0] != digest:
                    raise RuntimeError('{} differs between {} and {}'.format(
                        name, seen[name][1], jar))


def _write_project(path, *, sdk_dir, modules, baseline=None):
    """Writes a project.xml like lint.py's, so tracing takes the build's paths."""
    work_dir = os.path.dirname(path)
    cache_dir = os.path.join(work_dir, 'cache')
    os.makedirs(cache_dir, exist_ok=True)
    lines = ['<?xml version="1.0" ?>', '<project>']
    lines.append('  <root dir="{}"/>'.format(work_dir))
    lines.append('  <sdk dir="{}"/>'.format(sdk_dir))
    lines.append('  <cache dir="{}"/>'.format(cache_dir))
    lines += modules
    if baseline:
        lines.append('  <baseline file="{}"/>'.format(baseline))
    lines.append('</project>')
    pathlib.Path(path).write_text('\n'.join(lines) + '\n', encoding='utf-8')


def _sample_workloads(work_dir, aar_dirs, classpath, sdk_dir,
                      jdk_dir):
    """Writes a sample library + app and returns lint argv lists for them.

    The image only gets reflection metadata for code that runs while tracing,
    so the runs mirror every lint.py mode: a library analysis (--analyze-only),
    the app analysis, the report (--report-only, filtered by a baseline) and a
    monolithic run.
    """
    sample_dir = os.path.join(work_dir, 'sample')
    shutil.copytree(str(_THIS_DIR / 'sample'), sample_dir)
    res_dir = os.path.join(sample_dir, 'res')
    srcs = sorted(
        str(p) for p in pathlib.Path(sample_dir, 'java').glob('**/*')
        if p.suffix in ('.java', '.kt'))
    resources = sorted(
        str(p) for p in pathlib.Path(res_dir).glob('**/*') if p.is_file())
    manifest = os.path.join(sample_dir, 'AndroidManifest.xml')

    def module(name, *, library, partials, src=True, res=False):
        attrs = 'name="{}" android="true" library="{}"'.format(
            name, 'true' if library else 'false')
        if partials:
            attrs += ' partial-results-dir="{}"'.format(partials)
        lines = ['  <module {}>'.format(attrs)]
        lines.append('    <merged-manifest file="{}"/>'.format(manifest))
        if src:
            lines += ['    <src file="{}"/>'.format(s) for s in srcs]
        if res:
            lines += [
                '    <resource file="{}"/>'.format(r) for r in resources
            ]
        lines += ['    <classpath jar="{}"/>'.format(c) for c in classpath]
        lines += [
            '    <aar file="{}" extracted="{}"/>'.format(a, d)
            for a, d in aar_dirs
        ]
        return lines

    lib_partials = os.path.join(work_dir, 'lib_partials')
    app_partials = os.path.join(work_dir, 'app_partials')
    full_partials = os.path.join(work_dir, 'full_partials')
    for d in (lib_partials, app_partials, full_partials):
        os.makedirs(d, exist_ok=True)
    # lint.py passes a baseline to the report step.
    baseline = os.path.join(work_dir, 'lint-baseline.xml')
    pathlib.Path(baseline).write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<issues format="6" by="lint">\n</issues>\n',
        encoding='utf-8')
    lib_project = os.path.join(work_dir, 'lib_project.xml')
    app_project = os.path.join(work_dir, 'app_project.xml')
    report_project = os.path.join(work_dir, 'report_project.xml')
    full_project = os.path.join(work_dir, 'full_project.xml')
    _write_project(lib_project,
                   sdk_dir=sdk_dir,
                   modules=module('lib', library=True, partials=lib_partials) +
                   ['  </module>'])
    _write_project(app_project,
                   sdk_dir=sdk_dir,
                   modules=module('main',
                                  library=False,
                                  partials=app_partials,
                                  src=False,
                                  res=True) + ['  </module>'])
    report_modules = module(
        'main', library=False, partials=app_partials, src=False, res=True)
    report_modules += ['    <dep module="lib"/>', '  </module>']
    report_modules += [
        '  <module name="lib" android="true" library="true" '
        'partial-results-dir="{}"/>'.format(lib_partials)
    ]
    _write_project(report_project,
                   sdk_dir=sdk_dir,
                   modules=report_modules,
                   baseline=baseline)
    _write_project(full_project,
                   sdk_dir=sdk_dir,
                   modules=module('main',
                                  library=False,
                                  partials=full_partials,
                                  res=True) + ['  </module>'])
    # Chromium's lint-suppressions.xml ignores issues by path regexp.
    config = os.path.join(work_dir, 'config.xml')
    pathlib.Path(config).write_text(
        '<?xml version="1.0" ?>\n<lint>\n  <issue id="all">\n'
        '    <ignore regexp="^ignored/"/>\n  </issue>\n</lint>\n',
        encoding='utf-8')
    common_args = [
        '--sdk-home', sdk_dir, '--jdk-home', jdk_dir, '--path-variables',
        'SRC={}'.format(work_dir), '--offline', '--quiet', '--stacktrace',
        '--config', config,
        # Their JoinEffectDetector crashes in this partial analysis.
        # lint.py disables ThreadConstraint too (b/496928954).
        '--disable',
        'ThreadConstraint,BlockingMethod,StaticInitializerThreadConstraint,'
        'BlockingStaticInitializer'
    ]
    return [
        common_args + ['--project', lib_project, '--analyze-only'],
        common_args + ['--project', app_project, '--analyze-only'],
        common_args + ['--project', report_project, '--report-only'],
        common_args + [
            '--project', full_project, '--xml',
            os.path.join(work_dir, 'full.xml')
        ],
    ], lib_partials


def _native_image_metadata(lint_jar, aar_lint_jars):
    """Registrations the tracing runs cannot be relied on to record."""
    ctor_types = set()
    detector_types = set()
    resources = []
    psi_re = re.compile(r'(org/jetbrains/kotlin/(psi|kdoc/psi)|'
                        r'com/intellij/psi/impl/source)/[\w/$]*\.class')
    checks_re = re.compile(r'com/android/tools/lint/checks/\w+\.class')
    bundle_re = re.compile(r'(com/intellij|org/jetbrains)/[\w/]*Bundle\.class')
    # Parser error messages (JavaParserUtil.expectOrError) are read through
    # AbstractBundle.getMessage via reflection, which only Java code with
    # syntax errors reaches.
    bundle_types = set()
    with zipfile.ZipFile(lint_jar) as z:
        names = z.namelist()
        classes = {
            n[:-6].replace('/', '.')
            for n in names if n.endswith('.class')
        }
        for name in names:
            if psi_re.fullmatch(name) and not re.search(r'\$\d', name):
                # PSI elements are created from their (ASTNode) constructors.
                ctor_types.add(name[:-6].replace('/', '.'))
            elif checks_re.fullmatch(name):
                # Detectors are created from their default constructors.
                detector_types.add(name[:-6].replace('/', '.'))
            elif bundle_re.fullmatch(name):
                bundle_types.add(name[:-6].replace('/', '.'))
            elif name.endswith('.xml') and not name.startswith('com/android/'):
                # IntelliJ services and extensions named in plugin XML files.
                text = z.read(name).decode('utf-8', 'replace')
                for c in re.findall(r'="([a-z]\w*(?:\.[\w$]+)+)"', text):
                    if c in classes:
                        ctor_types.add(c)
            if not (name.endswith(('/', '.class')) or name.startswith(
                ('com/sun/jna/', 'org/bouncycastle/', 'META-INF/native-image/'))
                    or re.search(r'[*?{}\[\]\\]', name)):
                # Data files read only when a check fires (private-apis.txt).
                resources.append({'glob': name})
    for jar in aar_lint_jars:
        with zipfile.ZipFile(jar) as z:
            for name in z.namelist():
                if name.endswith('.class'):
                    detector_types.add(name[:-6].replace('/', '.'))
                elif not name.endswith('/') and not name.startswith('META-INF/'):
                    resources.append({'glob': name})
    reflection = [{
        'type': t,
        'allDeclaredConstructors': True,
        'allPublicMethods': True,
    } for t in sorted(detector_types | bundle_types)]
    reflection += [{
        'type': t,
        'allDeclaredConstructors': True
    } for t in sorted(ctor_types - detector_types - bundle_types)]
    # Stub element types create PSI arrays (e.g. KtModifierList[]).
    reflection += [{
        'type': t + '[]'
    } for t in sorted(ctor_types) if t.startswith('org.jetbrains.kotlin.psi.')]
    reflection += [{
        'type': t
    } for t in ('java.util.Iterator', 'java.util.ResourceBundle')]
    return {'reflection': reflection, 'resources': resources}


def _build_native_image(lint_jar, output_prefix):
    work_dir = os.path.abspath('native_work')
    # Left behind by a failed local-test run.
    shutil.rmtree(work_dir, ignore_errors=True)
    os.makedirs(work_dir)
    graal_home = graalvm.ensure_graalvm()
    src_root = common.path_within_checkout('.')

    aars = [aar for aar, _ in _aars_with_lint_jars(src_root)]
    aar_dirs = _extract_aars(aars, os.path.join(work_dir, 'aars'))
    aar_lint_jars = [os.path.join(d, 'lint.jar') for _, d in aar_dirs]
    _check_lint_jar_conflicts(aar_lint_jars)
    classpath = [os.path.join(d, 'classes.jar') for _, d in aar_dirs]
    for path in _sample_classpath_files(src_root):
        if path.endswith('.aar'):
            classpath += [
                os.path.join(d, 'classes.jar')
                for _, d in _extract_aars([path],
                                          os.path.join(work_dir, 'cp_aars'))
            ]
        else:
            classpath.append(path)
    classpath.append(os.path.join(src_root, _KOTLIN_STDLIB))
    classpath = [c for c in classpath if os.path.exists(c)]

    # The analysis API reads builtins through jar: URLs, which the image does
    # not have; Target_BuiltinsVirtualFileProviderCliImpl reads them from here.
    bin_dir = os.path.join(output_prefix, 'bin')
    os.makedirs(bin_dir, exist_ok=True)
    with zipfile.ZipFile(lint_jar) as src, zipfile.ZipFile(
            os.path.join(bin_dir, 'lint-builtins.jar'), 'w') as dst:
        for name in src.namelist():
            if name.endswith('.kotlin_builtins'):
                dst.writestr(name, src.read(name))

    subst_dir = os.path.join(work_dir, 'substitutions')
    common.run_cmd([
        os.path.join(graal_home, 'bin', 'javac'), '--add-exports',
        'org.graalvm.nativeimage/com.oracle.svm.core.annotate=ALL-UNNAMED',
        '-cp', lint_jar, '-d', subst_dir
    ] + sorted(str(p) for p in (_THIS_DIR / 'substitutions').glob('*.java')))

    sdk_platform = _sdk_platform(src_root)
    sdk_dir = os.path.dirname(os.path.dirname(sdk_platform))
    workloads, lib_partials = _sample_workloads(
        work_dir, aar_dirs,
        classpath + [os.path.join(sdk_platform, 'android.jar')], sdk_dir,
        graal_home)
    # Lint writes an analytics.settings file here (as in lint.py).
    os.environ['ANDROID_SDK_HOME'] = os.path.join(work_dir, 'android_home')
    output_path = os.path.join(bin_dir, 'lint-graalvm')
    graalvm.build_native_image(
        lint_jar,
        output_path,
        main_class=_MAIN_CLASS,
        extra_cp=[subst_dir] + aar_lint_jars,
        tracing_args_list=workloads,
        extra_reachability_metadata=_native_image_metadata(
            lint_jar, aar_lint_jars),
        register_all_fields=True,
        copy_shared_libs=True,
        extra_native_image_args=[
            # lint.jar bundles configs for opentelemetry and jline that do not
            # match its own versions of them.
            '--exclude-config',
            r'lint\.jar',
            r'META-INF/native-image/.*',
            # Silence JDK 24+ warnings (as kotlinc's 3pp does).
            '--enable-native-access=ALL-UNNAMED',
            '-J--sun-misc-unsafe-memory-access=allow',
            # Its <clinit> looks up charsets that are not included by default.
            '--initialize-at-build-time='
            'com.intellij.openapi.vfs.CharsetToolkit',
            '-H:+UnlockExperimentalVMOptions',
            # A Jimfs provider would otherwise be stored in the image heap.
            '-H:-AddAllFileSystemProviders',
            # Lint reads the JDK given by --jdk-home through jrt:/.
            '-H:+AllowJRTFileSystem',
            # Collects in parallel; lint.py caps it with
            # -XX:ActiveProcessorCount=2.
            '--gc=G1',
            # The 3pp packager has 32 GB of RAM; the build peaks at ~14 GB.
            '-J-Xmx20g',
        ],
    )

    # Smoke test: the image and the JVM must write the same partial results
    # for the sample library.
    jvm_partials = os.path.join(work_dir, 'jvm_partials')
    shutil.copytree(lib_partials, jvm_partials)
    shutil.rmtree(lib_partials)
    os.makedirs(lib_partials)
    # Like the tracing runs, lint may exit non-zero when it finds issues.
    common.run_cmd([output_path, '-XX:ActiveProcessorCount=2'] + workloads[0],
                   check=False)
    if not os.listdir(jvm_partials):
        raise RuntimeError('lint-graalvm smoke test: no JVM partial results')
    if sorted(os.listdir(jvm_partials)) != sorted(os.listdir(lib_partials)):
        raise RuntimeError('lint-graalvm smoke test: different partial files')
    for name in sorted(os.listdir(jvm_partials)):
        a = pathlib.Path(jvm_partials, name).read_bytes()
        b = pathlib.Path(lib_partials, name).read_bytes()
        # A detector crash that the JVM has too is a lint bug, not an image
        # bug, but its stack trace differs in the image.
        if b'LintError' in a:
            a, b = (re.sub(rb'Stack: `[^`]*`', b'', x) for x in (a, b))
        if a != b:
            raise RuntimeError('lint-graalvm smoke test: {} differs'.format(name))
        if b'LintError' in b and b'LintError' not in a:
            raise RuntimeError('lint-graalvm smoke test: LintError in ' + name)
    shutil.rmtree(work_dir)


def _post_process(src_jar_path, dst_jar_path):
    shutil.move(src_jar_path, dst_jar_path)
    _build_native_image(os.path.abspath(dst_jar_path),
                        os.path.dirname(os.path.abspath(dst_jar_path)))


maven.main(
    package='com.android.tools.lint:lint',
    post_process_func=_post_process,
    # Allows local-test runs against the lint version in //DEPS.
    version_override=os.environ.get('LINT_3PP_VERSION_OVERRIDE'),
    runtime_deps=_runtime_deps(),
    version_deps=['//third_party/android_build_tools/lint/3pp'],
    version_strings_func=_version_strings,
)
