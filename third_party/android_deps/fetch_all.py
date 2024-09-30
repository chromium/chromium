#!/usr/bin/env python3

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script used to manage Google Maven dependencies for Chromium.

For each dependency in `build.gradle`:

  - Download the library
  - Download the LICENSE
  - Generate a README.chromium file
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate a 'deps' entry in DEPS.
"""

import argparse
import collections
import concurrent.futures
import contextlib
import fnmatch
import logging
import tempfile
import textwrap
import os
import re
import shutil
import subprocess
import urllib.request
import zipfile

# Assume this script is stored under third_party/android_deps/
_CHROMIUM_SRC = os.path.normpath(os.path.join(__file__, '..', '..', '..'))

# Default android_deps directory.
_PRIMARY_ANDROID_DEPS_DIR = os.path.join(_CHROMIUM_SRC, 'third_party',
                                         'android_deps')

# Path to additional_readme_paths.json relative to custom 'android_deps' directory.
_ADDITIONAL_README_PATHS = 'additional_readme_paths.json'

# Path to BUILD.gn file from custom 'android_deps' directory.
_BUILD_GN = 'BUILD.gn'

# Path to build.gradle file relative to custom 'android_deps' directory.
_BUILD_GRADLE = 'build.gradle'

# Location of the android_deps libs directory relative to custom 'android_deps' directory.
_LIBS_DIR = 'libs'

_GN_PATH = os.path.join(_CHROMIUM_SRC, 'third_party', 'depot_tools', 'gn')

_GRADLEW = os.path.join(_CHROMIUM_SRC, 'third_party', 'android_build_tools',
                        'gradle_wrapper', 'gradlew')

_JAVA_HOME = os.path.join(_CHROMIUM_SRC, 'third_party', 'jdk', 'current')

# Git-controlled files needed by, but not updated by this tool.
# Relative to _PRIMARY_ANDROID_DEPS_DIR.
_PRIMARY_ANDROID_DEPS_FILES = [
    'buildSrc',
    'licenses',
    'settings.gradle.template',
    'vulnerability_supressions.xml',
]

# Git-controlled files needed by and updated by this tool.
# Relative to args.android_deps_dir.
_CUSTOM_ANDROID_DEPS_FILES = [
    os.path.join('..', '..', 'DEPS'),
    _BUILD_GN,
    _ADDITIONAL_README_PATHS,
    'subprojects.txt',
]

# Dictionary mapping long info file names to shorter ones to avoid paths being
# over 200 chars. This must match the dictionary in BuildConfigGenerator.groovy.
_REDUCED_ID_LENGTH_MAP = {
    'com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework':
    'com_google_android_accessibility_test_framework',
}

# If this file exists in an aar file then it is appended to LICENSE
_THIRD_PARTY_LICENSE_FILENAME = 'third_party_licenses.txt'

# Path to the aar.py script used to generate .info files.
_AAR_PY = os.path.join(_CHROMIUM_SRC, 'build', 'android', 'gyp', 'aar.py')


@contextlib.contextmanager
def BuildDir(dirname=None):
    """Helper function used to manage a build directory.

    Args:
      dirname: Optional build directory path. If not provided, a temporary
        directory will be created and cleaned up on exit.
    Returns:
      A python context manager modelling a directory path. The manager
      removes the directory if necessary on exit.
    """
    delete = False
    if not dirname:
        dirname = tempfile.mkdtemp()
        delete = True
    try:
        yield dirname
    finally:
        if delete:
            shutil.rmtree(dirname)


def RaiseCommandException(args, returncode, output, error):
    """Raise an exception whose message describing a command failure.

  Args:
    args: shell command-line (as passed to subprocess.call())
    returncode: status code.
    error: standard error output.
  Raises:
    a new Exception.
  """
    message = 'Command failed with status {}: {}\n'.format(returncode, args)
    if output:
        message += 'Output:-----------------------------------------\n{}\n' \
            '------------------------------------------------\n'.format(output)
    if error:
        message += 'Error message: ---------------------------------\n{}\n' \
            '------------------------------------------------\n'.format(error)
    raise Exception(message)


def RunCommand(args, print_stdout=False, cwd=None):
    """Run a new shell command.

  This function runs without printing anything.

  Args:
    args: A string or a list of strings for the shell command.
  Raises:
    On failure, raise an Exception that contains the command's arguments,
    return status, and standard output + error merged in a single message.
  """
    logging.debug('Run %s', args)
    stdout = None if print_stdout else subprocess.PIPE
    # Explicitly set JAVA_HOME since some bots do not have this already set.
    env = os.environ.copy()
    env['JAVA_HOME'] = _JAVA_HOME
    p = subprocess.Popen(args, stdout=stdout, cwd=cwd, env=env)
    pout, _ = p.communicate()
    if p.returncode != 0:
        RaiseCommandException(args, p.returncode, None, pout)


def RunCommandAndGetOutput(args):
    """Run a new shell command. Return its output. Exception on failure.

  This function runs without printing anything.

  Args:
    args: A string or a list of strings for the shell command.
  Returns:
    The command's output.
  Raises:
    On failure, raise an Exception that contains the command's arguments,
    return status, and standard output, and standard error as separate
    messages.
  """
    logging.debug('Run %s', args)
    # Explicitly set JAVA_HOME since some bots do not have this already set.
    env = os.environ.copy()
    env['JAVA_HOME'] = _JAVA_HOME
    p = subprocess.Popen(args,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         env=env)
    pout, perr = p.communicate()
    if p.returncode != 0:
        RaiseCommandException(args, p.returncode, pout, perr)
    return pout


def MakeDirectory(dir_path):
    """Make directory |dir_path| recursively if necessary."""
    if dir_path != '' and not os.path.isdir(dir_path):
        logging.debug('mkdir [%s]', dir_path)
        os.makedirs(dir_path)


def DeleteDirectory(dir_path):
    """Recursively delete a directory if it exists."""
    if os.path.exists(dir_path):
        logging.debug('rmdir [%s]', dir_path)
        shutil.rmtree(dir_path)


def Copy(src_dir, src_paths, dst_dir, dst_paths, src_path_must_exist=True):
    """Copies |src_paths| in |src_dir| to |dst_paths| in |dst_dir|.

    Args:
      src_dir: Directory containing |src_paths|.
      src_paths: Files to copy.
      dst_dir: Directory containing |dst_paths|.
      dst_paths: Copy destinations.
      src_paths_must_exist: If False, do not throw error if the file for one of
          |src_paths| does not exist.
    """
    assert len(src_paths) == len(dst_paths)

    missing_files = []
    for src_path, dst_path in zip(src_paths, dst_paths):
        abs_src_path = os.path.join(src_dir, src_path)
        abs_dst_path = os.path.join(dst_dir, dst_path)
        if os.path.exists(abs_src_path):
            CopyFileOrDirectory(abs_src_path, abs_dst_path)
        elif src_path_must_exist:
            missing_files.append(src_path)

    if missing_files:
        raise Exception('Missing files from {}: {}'.format(
            src_dir, missing_files))


def CopyFileOrDirectory(src_path, dst_path, ignore_extension=None):
    """Copy file or directory |src_path| into |dst_path| exactly.

    Args:
      src_path: Source path.
      dst_path: Destination path.
      ignore_extension: File extension of files not to copy, starting with '.'. If None, all files
          are copied.
    """
    assert not ignore_extension or ignore_extension[0] == '.'

    src_path = os.path.normpath(src_path)
    dst_path = os.path.normpath(dst_path)
    logging.debug('copy [%s -> %s]', src_path, dst_path)
    MakeDirectory(os.path.dirname(dst_path))
    if os.path.isdir(src_path):
        # Copy directory recursively.
        DeleteDirectory(dst_path)
        ignore = None
        if ignore_extension:
            ignore = shutil.ignore_patterns('*' + ignore_extension)
        shutil.copytree(src_path, dst_path, ignore=ignore)
    elif not ignore_extension or not re.match('.*\.' + ignore_extension[1:],
                                              src_path):
        shutil.copy(src_path, dst_path)


def ReadFile(file_path):
    """Read a file, return its content."""
    with open(file_path) as f:
        return f.read()


def ReadFileAsLines(file_path):
    """Read a file as a series of lines."""
    with open(file_path) as f:
        return f.readlines()


def WriteFile(file_path, file_data):
    """Write a file."""
    if isinstance(file_data, str):
        file_data = file_data.encode('utf8')
    MakeDirectory(os.path.dirname(file_path))
    with open(file_path, 'wb') as f:
        f.write(file_data)


def FindInDirectory(directory, filename_filter):
    """Find all files in a directory that matches a given filename filter."""
    files = []
    for root, _dirnames, filenames in os.walk(directory):
        matched_files = fnmatch.filter(filenames, filename_filter)
        files.extend((os.path.join(root, f) for f in matched_files))
    return files


# Named tuple describing a CIPD package.
# - path: Path to cipd.yaml file.
# - name: cipd package name.
# - tag: cipd tag.
CipdPackageInfo = collections.namedtuple('CipdPackageInfo',
                                         ['path', 'name', 'tag'])

# Regular expressions used to extract useful info from cipd.yaml files
# generated by Gradle. See BuildConfigGenerator.groovy:makeCipdYaml()
_RE_CIPD_CREATE = re.compile('cipd create --pkg-def cipd.yaml -tag (\S*)')
_RE_CIPD_PACKAGE = re.compile('package: (\S*)')


def _ParseSubprojects(subproject_path):
    """Parses listing of subproject build.gradle files. Returns list of paths."""
    if not os.path.exists(subproject_path):
        return None

    subprojects = []
    for subproject in open(subproject_path):
        subproject = subproject.strip()
        if subproject and not subproject.startswith('#'):
            subprojects.append(subproject)
    return subprojects


def _GenerateSettingsGradle(subproject_dirs, settings_template_path,
                            settings_out_path):
    """Generates settings file by replacing "{{subproject_dirs}}" string in template.

    Args:
      subproject_dirs: List of subproject directories to substitute into template.
      settings_template_path: Path of template file to substitute into.
      settings_out_path: Path of output settings.gradle file.
    """
    with open(settings_template_path) as f:
        template_content = f.read()

    subproject_dirs_str = ''
    if subproject_dirs:
        subproject_dirs_str = '\'' + '\',\''.join(subproject_dirs) + '\''

    template_content = template_content.replace('{{subproject_dirs}}',
                                                subproject_dirs_str)
    with open(settings_out_path, 'w') as f:
        f.write(template_content)


def _BuildGradleCmd(build_android_deps_dir, task):
    return [
        _GRADLEW, '-p', build_android_deps_dir, '--stacktrace',
        '--warning-mode', 'all', task
    ]


def _CheckVulnerabilities(build_android_deps_dir, report_dst):
    logging.warning('Running Gradle dependencyCheckAnalyze. This may take a '
                    'few minutes the first time.')

    # Separate command from main gradle command so that we can provide specific
    # diagnostics in case of failure of this step.
    gradle_cmd = _BuildGradleCmd(build_android_deps_dir,
                                 'dependencyCheckAnalyze')

    report_src = os.path.join(build_android_deps_dir, 'build', 'reports')
    if os.path.exists(report_dst):
        shutil.rmtree(report_dst)

    try:
        logging.info('CMD: %s', ' '.join(gradle_cmd))
        RunCommand(gradle_cmd, print_stdout=True)
    except Exception:
        report_path = os.path.join(report_dst, 'dependency-check-report.html')
        logging.error(
            textwrap.dedent("""
               =============================================================================
               A package has a known vulnerability. It may not be in a package or packages
               which you just added, but you need to resolve the problem before proceeding.
               If you can't easily fix it by rolling the package to a fixed version now,
               please file a crbug of type= Bug-Security providing all relevant information,
               and then rerun this command with --ignore-vulnerabilities.
               The html version of the report is avialable at: {}
               =============================================================================
               """.format(report_path)))
        raise
    finally:
        if os.path.exists(report_src):
            CopyFileOrDirectory(report_src, report_dst)


def _ReduceNameLength(path_str):
    """Returns a shorter path string if needed.

  Args:
    path_str: A String representing the path.
  Returns:
    A String (possibly shortened) of that path.
  """
    return _REDUCED_ID_LENGTH_MAP.get(path_str, path_str)


def GetCipdPackageInfo(cipd_yaml_path):
    """Returns the CIPD package name corresponding to a given cipd.yaml file.

  Args:
    cipd_yaml_path: Path of input cipd.yaml file.
  Returns:
    A (package_name, package_tag) tuple.
  Raises:
    Exception if the file could not be read.
  """
    package_name = None
    package_tag = None
    for line in ReadFileAsLines(cipd_yaml_path):
        m = _RE_CIPD_PACKAGE.match(line)
        if m:
            package_name = m.group(1)

        m = _RE_CIPD_CREATE.search(line)
        if m:
            package_tag = m.group(1)

    if not package_name or not package_tag:
        raise Exception('Invalid cipd.yaml format: ' + cipd_yaml_path)

    return (package_name, package_tag)


def ParseDeps(root_dir, libs_dir):
    """Parse an android_deps/libs and retrieve package information.

  Args:
    root_dir: Path to a root Chromium or build directory.
  Returns:
    A directory mapping package names to tuples of
    (cipd_yaml_file, package_name, package_tag), where |cipd_yaml_file|
    is the path to the cipd.yaml file, related to |libs_dir|,
    and |package_name| and |package_tag| are the extracted from it.
  """
    result = {}
    root_dir = os.path.abspath(root_dir)
    libs_dir = os.path.abspath(os.path.join(root_dir, libs_dir))
    for cipd_file in FindInDirectory(libs_dir, 'cipd.yaml'):
        pkg_name, pkg_tag = GetCipdPackageInfo(cipd_file)
        cipd_path = os.path.dirname(cipd_file)
        cipd_path = cipd_path[len(root_dir) + 1:]
        result[pkg_name] = CipdPackageInfo(cipd_path, pkg_name, pkg_tag)

    return result


def PrintPackageList(packages, list_name):
    """Print a list of packages to standard output.

  Args:
    packages: list of package names.
    list_name: a simple word describing the package list (e.g. 'new')
  """
    print('  {} {} packages:'.format(len(packages), list_name))
    print('\n'.join('    - ' + p for p in packages))


def _DownloadOverrides(overrides, build_libs_dir):
    for spec in overrides:
        subpath, url = spec.split(':', 1)
        target_path = os.path.join(build_libs_dir, subpath)
        if not os.path.isfile(target_path):
            found_files = 'Found instead:\n' + '\n'.join(
                FindInDirectory(os.path.dirname(target_path), '*'))
            raise Exception(
                f'Override path does not exist: {target_path}\n{found_files}')
        logging.info('Fetching override for %s', target_path)
        with urllib.request.urlopen(url) as response:
            with open(target_path, 'wb') as f:
                shutil.copyfileobj(response, f)


def _CreateAarInfos(aar_files):
    jobs = []

    for aar_file in aar_files:
        aar_dirname = os.path.dirname(aar_file)
        aar_info_name = _ReduceNameLength(
            os.path.basename(aar_dirname)) + '.info'
        aar_info_path = os.path.join(aar_dirname, aar_info_name)

        logging.debug('- %s', aar_info_name)
        cmd = [_AAR_PY, 'list', aar_file, '--output', aar_info_path]

        if aar_info_name == 'com_google_android_material_material.info':
            # Keep in sync with copy in BuildConfigGenerator.groovy.
            resource_exclusion_glbos = [
                'res/layout*/*calendar*',
                'res/layout*/*chip_input*',
                'res/layout*/*clock*',
                'res/layout*/*picker*',
                'res/layout*/*time*',
            ]
            cmd += [
                '--resource-exclusion-globs',
                repr(resource_exclusion_glbos).replace("'", '"')
            ]
        proc = subprocess.Popen(cmd)
        jobs.append((cmd, proc))

    for cmd, proc in jobs:
        if proc.wait():
            raise Exception('Command Failed: {}\n'.format(' '.join(cmd)))


def _CopyJarFilesToCipd(android_deps_dir):
    src_libs_dir = os.path.join(android_deps_dir, _LIBS_DIR)
    # Match .aar and .jar
    for src_path in FindInDirectory(src_libs_dir, '*.?ar'):
        dst_path = os.path.join(android_deps_dir, 'cipd', _LIBS_DIR,
                                os.path.relpath(src_path, src_libs_dir))
        logging.debug('mv [%s -> %s]', src_path, dst_path)
        if os.path.exists(dst_path):
            os.unlink(dst_path)
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        shutil.move(src_path, dst_path)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        '--android-deps-dir',
        help='Path to directory containing build.gradle from chromium-dir.',
        default=_PRIMARY_ANDROID_DEPS_DIR)
    parser.add_argument(
        '--build-dir',
        help='Path to build directory (default is temporary directory).')
    parser.add_argument('--ignore-licenses',
                        help='Ignores licenses for these deps.',
                        action='store_true')
    parser.add_argument('--ignore-vulnerabilities',
                        help='Ignores vulnerabilities for these deps.',
                        action='store_true')
    parser.add_argument('--override-artifact',
                        action='append',
                        help='lib_subpath:url of .aar / .jar to override.')
    parser.add_argument('--no-subprojects',
                        action='store_true',
                        help='Ignore subprojects.txt for faster runs.')
    parser.add_argument('--local',
                        help='Move .jar and .aar files to cipd/ directory '
                        'after running (3pp bot requires this to not '
                        'happen)',
                        action='store_true')
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')
    debug = args.verbose_count >= 2

    if 'SWARMING_TASK_ID' not in os.environ and not args.local:
        logging.warning(
            'Detected not running on a bot. You probably want to use --local')

    if not os.path.isfile(os.path.join(args.android_deps_dir, _BUILD_GRADLE)):
        raise Exception('--android-deps-dir {} does not contain {}.'.format(
            args.android_deps_dir, _BUILD_GRADLE))
    is_primary_android_deps = args.android_deps_dir == _PRIMARY_ANDROID_DEPS_DIR
    android_deps_subdir = os.path.relpath(args.android_deps_dir, _CHROMIUM_SRC)

    with BuildDir(args.build_dir) as build_dir:
        build_android_deps_dir = os.path.join(build_dir, android_deps_subdir)

        logging.info('Using build directory: %s', build_dir)
        Copy(_PRIMARY_ANDROID_DEPS_DIR, _PRIMARY_ANDROID_DEPS_FILES,
             build_android_deps_dir, _PRIMARY_ANDROID_DEPS_FILES)
        Copy(args.android_deps_dir, [_BUILD_GRADLE], build_android_deps_dir,
             [_BUILD_GRADLE])
        Copy(args.android_deps_dir,
             _CUSTOM_ANDROID_DEPS_FILES,
             build_android_deps_dir,
             _CUSTOM_ANDROID_DEPS_FILES,
             src_path_must_exist=is_primary_android_deps)

        if args.no_subprojects:
            subprojects = None
        else:
            subprojects = _ParseSubprojects(
                os.path.join(args.android_deps_dir, 'subprojects.txt'))
        subproject_dirs = []
        if subprojects:
            for (index, subproject) in enumerate(subprojects):
                subproject_dir = 'subproject{}'.format(index)
                Copy(args.android_deps_dir, [subproject],
                     build_android_deps_dir,
                     [os.path.join(subproject_dir, 'build.gradle')])
                subproject_dirs.append(subproject_dir)

        _GenerateSettingsGradle(
            subproject_dirs,
            os.path.join(_PRIMARY_ANDROID_DEPS_DIR,
                         'settings.gradle.template'),
            os.path.join(build_android_deps_dir, 'settings.gradle'))

        if not args.ignore_vulnerabilities:
            report_dst = os.path.join(args.android_deps_dir,
                                      'vulnerability_reports')
            _CheckVulnerabilities(build_android_deps_dir, report_dst)

        logging.info('Running Gradle.')

        # This gradle command generates the new DEPS and BUILD.gn files, it can
        # also handle special cases.
        # Edit BuildConfigGenerator.groovy#addSpecialTreatment for such cases.
        gradle_cmd = _BuildGradleCmd(build_android_deps_dir, 'setupRepository')
        if debug:
            gradle_cmd.append('--debug')
        if args.ignore_licenses:
            gradle_cmd.append('-PskipLicenses=true')

        RunCommand(gradle_cmd, print_stdout=True)

        logging.info('# Reformat %s.',
                     os.path.join(args.android_deps_dir, _BUILD_GN))
        gn_path = os.path.relpath(_GN_PATH, _CHROMIUM_SRC)
        gn_input = os.path.join(
            os.path.relpath(build_android_deps_dir, _CHROMIUM_SRC), _BUILD_GN)
        gn_args = [gn_path, 'format', gn_input]
        try:
            RunCommand(gn_args, print_stdout=debug, cwd=_CHROMIUM_SRC)
        except Exception:
            if os.path.exists(gn_input):
                shutil.copyfile(gn_input, '/tmp/gn-format-input')
                logging.warning('Saved GN input to /tmp/gn-format-input')
            raise

        build_libs_dir = os.path.join(build_android_deps_dir, _LIBS_DIR)
        if args.override_artifact:
            _DownloadOverrides(args.override_artifact, build_libs_dir)
        aar_files = FindInDirectory(build_libs_dir, '*.aar')

        logging.info('# Generate Android .aar info files.')
        _CreateAarInfos(aar_files)

        if not args.ignore_licenses:
            logging.info('# Looking for nested license files.')
            for aar_file in aar_files:
                # Play Services .aar files have embedded licenses.
                with zipfile.ZipFile(aar_file) as z:
                    if _THIRD_PARTY_LICENSE_FILENAME in z.namelist():
                        aar_dirname = os.path.dirname(aar_file)
                        license_path = os.path.join(aar_dirname, 'LICENSE')
                        # Make sure to append as we don't want to lose the
                        # existing license.
                        with open(license_path, 'ab') as f:
                            f.write(z.read(_THIRD_PARTY_LICENSE_FILENAME))

        logging.info('# Compare CIPD packages.')
        existing_packages = ParseDeps(args.android_deps_dir, _LIBS_DIR)
        build_packages = ParseDeps(build_android_deps_dir, _LIBS_DIR)

        deleted_packages = []
        updated_packages = []
        for pkg in sorted(existing_packages):
            if pkg not in build_packages:
                deleted_packages.append(pkg)
            else:
                existing_info = existing_packages[pkg]
                build_info = build_packages[pkg]
                if existing_info.tag != build_info.tag:
                    updated_packages.append(pkg)

        new_packages = sorted(set(build_packages) - set(existing_packages))

        # Copy updated DEPS and BUILD.gn to build directory.
        Copy(build_android_deps_dir,
             _CUSTOM_ANDROID_DEPS_FILES,
             args.android_deps_dir,
             _CUSTOM_ANDROID_DEPS_FILES,
             src_path_must_exist=is_primary_android_deps)

        # Delete obsolete or updated package directories.
        for pkg in existing_packages.values():
            pkg_path = os.path.join(args.android_deps_dir, pkg.path)
            DeleteDirectory(pkg_path)

        # Copy new and updated packages from build directory.
        for pkg in build_packages.values():
            pkg_path = pkg.path
            dst_pkg_path = os.path.join(args.android_deps_dir, pkg_path)
            src_pkg_path = os.path.join(build_android_deps_dir, pkg_path)
            CopyFileOrDirectory(src_pkg_path,
                                dst_pkg_path,
                                ignore_extension=".tmp")

        if args.local:
            _CopyJarFilesToCipd(args.android_deps_dir)

        # Useful for printing timestamp.
        logging.info('All Done.')

        if new_packages:
            PrintPackageList(new_packages, 'new')
        if updated_packages:
            PrintPackageList(updated_packages, 'updated')
        if deleted_packages:
            PrintPackageList(deleted_packages, 'deleted')


if __name__ == "__main__":
    main()
