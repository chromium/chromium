#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages Error Prone, NullAway, and a native-image of javac + Error Prone.

The native-image (error_prone) has NullAway and Chromium's Error Prone checks
(//tools/android/errorprone_plugin) compiled in, since native-images cannot
load plugins dynamically.
"""

import os
import pathlib
import re
import shutil
import subprocess
import sys
import zipfile

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[3 if _THIS_DIR.name == '3pp' else 2]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import common
import graalvm
import maven

_NULLAWAY_PACKAGE = 'com.uber.nullaway:nullaway'
_PLUGIN_DIR = 'tools/android/errorprone_plugin'
_SERVICE_IMPL_JAVA = (
    'build/android/java/src/org/chromium/build/annotations/ServiceImpl.java')

# Packages that Error Prone accesses. From:
# https://errorprone.info/docs/installation
_JAVAC_EXPORTS = [
    'jdk.compiler/com.sun.tools.javac.' + p for p in (
        'api',
        'file',
        'main',
        'model',
        'parser',
        'processing',
        'tree',
        'util',
    )
]
_JAVAC_OPENS = [
    'jdk.compiler/com.sun.tools.javac.code',
    'jdk.compiler/com.sun.tools.javac.comp',
]

_SAMPLE_JAVA = """\
package org.chromium.sample;

import java.util.List;
import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

@NullMarked
public class Sample {
    public static long countItems(List<String> items) {
        return items.stream().count();
    }

    public static int length(@Nullable String value) {
        return value.length();
    }
}
"""


def _filter_nullaway_jar(src_jar_path, dst_jar_path):
    with zipfile.ZipFile(src_jar_path) as in_z:
        with zipfile.ZipFile(dst_jar_path, 'w') as out_z:
            for info in in_z.infolist():
                if info.is_dir():
                    continue
                # Remove classes that already exist within error_prone_core.jar.
                if info.filename.startswith('com/google'):
                    continue
                out_z.writestr(info, in_z.read(info))


def _plugin_sources():
    """Returns the plugin's .java files that are listed in its BUILD.gn."""
    plugin_dir = common.path_within_checkout(_PLUGIN_DIR)
    build_gn = pathlib.Path(plugin_dir, 'BUILD.gn').read_text(encoding='utf-8')
    # Ignores commented-out sources.
    paths = re.findall(r'^\s*"(src/[^"]+\.java)",', build_gn, re.MULTILINE)
    if not paths:
        raise RuntimeError('Failed to parse sources from ' + pathlib.Path(plugin_dir, 'BUILD.gn'))
    return [os.path.join(plugin_dir, p) for p in paths]


def _build_plugin_jar(classpath, output_jar):
    """Compiles //tools/android/errorprone_plugin into |output_jar|."""
    sources = _plugin_sources()
    classes_dir = os.path.abspath('errorprone_plugin_classes')
    cmd = [
        common.path_within_checkout('third_party/jdk/current/bin/javac'),
        # Not --release, since that disallows --add-exports.
        '-source',
        '25',
        '-target',
        '25',
        '-proc:none',
        '-d',
        classes_dir,
        '-cp',
        os.pathsep.join(classpath),
    ]
    cmd += [
        '--add-exports=' + x + '=ALL-UNNAMED'
        for x in _JAVAC_EXPORTS + _JAVAC_OPENS
    ]
    cmd += sources + [common.path_within_checkout(_SERVICE_IMPL_JAVA)]
    common.run_cmd(cmd)

    # Create META-INF/services entries for @ServiceImpl (as compile_java.py
    # does).
    services = {}
    for path in sources:
        data = pathlib.Path(path).read_text(encoding='utf-8')
        m = re.search(r'@ServiceImpl\((\w+)\.class\)', data)
        if not m:
            continue
        iface = m.group(1)
        iface_fqn = re.search(r'^import ([\w.]+\.' + iface + r');', data,
                              re.MULTILINE).group(1)
        package = re.search(r'^package ([\w.]+);', data, re.MULTILINE).group(1)
        impl = package + '.' + os.path.basename(path)[:-len('.java')]
        services.setdefault(iface_fqn, []).append(impl)

    with zipfile.ZipFile(output_jar, 'w') as z:
        for p in sorted(pathlib.Path(classes_dir).glob('**/*.class')):
            # ServiceImpl is a build-time-only annotation.
            if p.name == 'ServiceImpl.class':
                continue
            z.write(str(p), p.relative_to(classes_dir).as_posix())
        for iface, impls in sorted(services.items()):
            z.writestr(
                'META-INF/services/' + iface,
                ''.join(x + '\n' for x in sorted(impls)),
            )


def _error_prone_args(xep_flags, extra_args):
    """Returns javac args similar to those used by errorprone.py."""
    return [
        '-XDcompilePolicy=simple',
        '-XDshould-stop.ifError=FLOW',
        '-XDshould-stop.ifNoError=FLOW',
        '-proc:none',
        ' '.join(['-Xplugin:ErrorProne', '-XepAllErrorsAsWarnings'] +
                 xep_flags),
    ] + extra_args


def _build_native_image(error_prone_jar, nullaway_jar, plugin_jar,
                        output_path):
    work_dir = os.path.abspath('native_image_inputs')
    os.makedirs(work_dir)
    sample_java = os.path.join(work_dir, 'Sample.java')
    pathlib.Path(sample_java).write_text(_SAMPLE_JAVA, encoding='utf-8')
    # Error Prone finds NullAway & Chromium's checks via ServiceLoader on the
    # -processorpath (as is done by errorprone.py). NullAway's jar also contains
    # the jspecify annotations.
    processorpath = os.pathsep.join([nullaway_jar, plugin_jar])
    common_args = ['-processorpath', processorpath, '-cp', nullaway_jar]
    # NullAway fails to initialize unless either OnlyNullMarked or
    # AnnotatedPackages is set.
    nullaway_flags = [
        '-XepOpt:NullAway:OnlyNullMarked=true',
        '-XepOpt:NullAway:JSpecifyMode=true',
    ]

    # Compiling javac's sources (with NullAway enabled for them) is used as a
    # representative workload for profile-guided optimizations.
    pgo_src_dir = os.path.join(work_dir, 'pgo_src')
    # javac's sources are not @NullMarked, so use AnnotatedPackages to have
    # NullAway check them.
    pgo_nullaway_flags = [
        '-XepOpt:NullAway:AnnotatedPackages=com.sun.tools.javac',
        '-XepOpt:NullAway:JSpecifyMode=true',
        # NullAway crashes on this class (NPE in
        # CheckIdenticalNullabilityVisitor), which aborts compilation of the
        # remaining sources.
        '-XepOpt:NullAway:ExcludedClasses=com.sun.tools.javac.comp.Infer',
    ]

    def pgo_args(out_dir):
        return [
            '-d',
            os.path.join(work_dir, out_dir),
            '--module-source-path',
            pgo_src_dir,
            '--module',
            'jdk.compiler',
        ]

    graalvm.extract_jdk_sources(pgo_src_dir,
                                prefix=('jdk.compiler/', 'java.compiler/'),
                                recursive=True)

    def sample_args(out_dir):
        return [
            '--release',
            '25',
            '-d',
            os.path.join(work_dir, out_dir),
            sample_java,
        ]

    graalvm.build_native_image(
        error_prone_jar,
        output_path,
        main_class='com.sun.tools.javac.Main',
        extra_cp=[nullaway_jar, plugin_jar],
        add_exports=_JAVAC_EXPORTS,
        add_opens=_JAVAC_OPENS,
        # Enable all checks so that they are all traced.
        tracing_args_list=[
            _error_prone_args(
                ['-XepAllDisabledChecksAsWarnings'] + nullaway_flags,
                common_args + sample_args('trace_out'),
            ),
            _error_prone_args(
                [
                    '-XepAllDisabledChecksAsWarnings',
                    # These crash when compiling javac's sources.
                    '-Xep:FieldCanBeFinal:OFF',
                    '-Xep:UnnecessaryDefaultInEnumSwitch:OFF',
                ] + pgo_nullaway_flags,
                common_args + pgo_args('trace_pgo_out'),
            ),
        ],
        pgo_args_list=[
            _error_prone_args(pgo_nullaway_flags,
                              common_args + pgo_args('pgo_out')),
        ],
        extra_native_image_args=[
            # Required to read lib/modules (for --release).
            '-H:+UnlockExperimentalVMOptions',
            '-H:+AllowJRTFileSystem',
            # org.safere.Pattern's <clinit> calls MutableCallSite.setTarget(),
            # which is not supported at runtime.
            '--initialize-at-build-time=org.safere',
        ],
        # Ensure the workloads run to completion (e.g. NullAway does not
        # crash part way through).
        check_workloads=True,
    )

    # Ensure that Chromium's checks and NullAway are both active.
    output = common.run_cmd(
        [output_path, '-Djava.home=' + graalvm.ensure_graalvm()] +
        _error_prone_args(
            nullaway_flags,
            common_args + sample_args('smoke_out'),
        ),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    ).stdout
    for check in ('[NoStreams]', '[NullAway]'):
        if check not in output:
            raise RuntimeError(
                f'error_prone smoke test did not report {check}:\n{output}')


def _post_process(src_jar_path, dst_jar_path):
    output_dir = os.path.dirname(dst_jar_path)
    shutil.move(src_jar_path, dst_jar_path)

    # The version is also looked up by _version_strings(). If a release happens
    # in between, the package will be rebuilt by the next 3pp run.
    nullaway_version = maven.latest_version(maven.APACHE_MAVEN_URL,
                                            _NULLAWAY_PACKAGE)
    nullaway_src_jar = maven.build_jar_with_dependencies(
        maven.APACHE_MAVEN_URL,
        _NULLAWAY_PACKAGE,
        nullaway_version,
        work_dir='nullaway_build',
    )
    nullaway_jar = os.path.join(output_dir, 'nullaway.jar')
    _filter_nullaway_jar(nullaway_src_jar, nullaway_jar)
    pathlib.Path(output_dir,
                 'NULLAWAY_VERSION').write_text(nullaway_version + '\n',
                                                encoding='utf-8')

    plugin_jar = os.path.abspath('errorprone_plugin.jar')
    _build_plugin_jar([dst_jar_path, nullaway_jar], plugin_jar)

    _build_native_image(
        dst_jar_path,
        nullaway_jar,
        plugin_jar,
        os.path.join(output_dir, 'error_prone'),
    )


def _version_strings():
    # Rebuild when either NullAway or GraalVM change.
    return [maven.latest_version(maven.APACHE_MAVEN_URL, _NULLAWAY_PACKAGE)
            ] + graalvm.version_strings()


maven.main(
    package='com.google.errorprone:error_prone_core',
    maven_url=maven.APACHE_MAVEN_URL,
    post_process_func=_post_process,
    runtime_deps=['//' + _PLUGIN_DIR, '//' + _SERVICE_IMPL_JAVA],
    version_deps=['//' + _PLUGIN_DIR, '//' + _SERVICE_IMPL_JAVA],
    version_strings_func=_version_strings,
)
