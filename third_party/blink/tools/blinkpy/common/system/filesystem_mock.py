# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import hashlib
import os
import re
import unittest

from blinkpy.common.system.filesystem import _remove_contents, _sanitize_filename

from six import ensure_binary, StringIO

_TEXT_ENCODING = 'utf-8'


def _ensure_binary_contents(file_contents):
    # Iterate over a copy while the underlying mapping is mutated.
    for path, contents in list(file_contents.items()):
        if contents is not None:
            contents = ensure_binary(contents, _TEXT_ENCODING)
        file_contents[path] = contents


class MockFileSystem(object):
    # pylint: disable=unused-argument

    sep = '/'
    pardir = '..'

    def __init__(self, files=None, dirs=None, cwd='/'):
        """Initializes a "mock" filesystem that can be used to replace the
        FileSystem class in tests.

        Args:
            files: A dictionary of filenames to file contents. A file contents
                value of None indicates that the file does not exist.
        """
        self.files = files or {}
        self.executable_files = set()
        self.written_files = {}
        self.last_tmpdir = None
        self.current_tmpno = 0
        self.cwd = cwd
        self.dirs = set(dirs or [])
        self.dirs.add(cwd)
        _ensure_binary_contents(self.files)
        for file_path in self.files:
            directory = self.dirname(file_path)
            while directory not in self.dirs:
                self.dirs.add(directory)
                directory = self.dirname(directory)

    def clear_written_files(self):
        # This function can be used to track what is written between steps in a test.
        self.written_files = {}

    def _raise_not_found(self, path):
        raise IOError(errno.ENOENT, path, os.strerror(errno.ENOENT))

    def _split(self, path):
        # This is not quite a full implementation of os.path.split; see:
        # http://docs.python.org/library/os.path.html#os.path.split
        if self.sep in path:
            return path.rsplit(self.sep, 1)
        return ('', path)

    def make_executable(self, file_path):
        self.executable_files.add(file_path)

    def abspath(self, path):
        if os.path.isabs(path):
            return self.normpath(path)
        return self.abspath(self.join(self.cwd, path))

    def realpath(self, path):
        return self.abspath(path)

    def basename(self, path):
        return self._split(path)[1]

    def expanduser(self, path):
        if path[0] != '~':
            return path
        parts = path.split(self.sep, 1)
        home_directory = self.sep + 'Users' + self.sep + 'mock'
        if len(parts) == 1:
            return home_directory
        return home_directory + self.sep + parts[1]

    def path_to_module(self, module_name):
        return ('/mock-checkout/third_party/blink/tools/' +
                module_name.replace('.', '/') + '.py')

    def chdir(self, path):
        path = self.normpath(path)
        if not self.isdir(path):
            raise OSError(errno.ENOENT, path, os.strerror(errno.ENOENT))
        self.cwd = path

    def copyfile(self, source, destination):
        if not self.exists(source):
            self._raise_not_found(source)
        if self.isdir(source):
            raise IOError(errno.EISDIR, source, os.strerror(errno.EISDIR))
        if self.isdir(destination):
            raise IOError(errno.EISDIR, destination, os.strerror(errno.EISDIR))
        if not self.exists(self.dirname(destination)):
            raise IOError(errno.ENOENT, destination, os.strerror(errno.ENOENT))

        self.files[destination] = self.files[source]
        self.written_files[destination] = self.files[source]

    def dirname(self, path):
        return self._split(path)[0]

    def exists(self, path):
        return self.isfile(path) or self.isdir(path)

    def files_under(self, path, dirs_to_skip=None, file_filter=None):
        dirs_to_skip = dirs_to_skip or []

        filter_all = lambda fs, dirpath, basename: True

        file_filter = file_filter or filter_all
        files = []
        if self.isfile(path):
            if (file_filter(self, self.dirname(path), self.basename(path))
                    and self.files[path] is not None):
                files.append(path)
            return files

        if self.basename(path) in dirs_to_skip:
            return []

        if not path.endswith(self.sep):
            path += self.sep

        dir_substrings = [self.sep + d + self.sep for d in dirs_to_skip]
        for filename in self.files:
            if not filename.startswith(path):
                continue

            suffix = filename[len(path) - 1:]
            if any(dir_substring in suffix
                   for dir_substring in dir_substrings):
                continue

            dirpath, basename = self._split(filename)
            if (file_filter(self, dirpath, basename)
                    and self.files[filename] is not None):
                files.append(filename)

        return files

    def getcwd(self):
        return self.cwd

    def glob(self, glob_string):
        # FIXME: This handles '*', but not '?', '[', or ']'.
        glob_string = re.escape(glob_string)
        glob_string = glob_string.replace('\\*', '[^\\/]*') + '$'
        glob_string = glob_string.replace('\\/', '/')
        path_filter = lambda path: re.match(glob_string, path)

        # We could use fnmatch.fnmatch, but that might not do the right thing on Windows.
        existing_files = [
            path for path, contents in self.files.items()
            if contents is not None
        ]
        return list(filter(path_filter, existing_files)) + list(
            filter(path_filter, self.dirs))

    def isabs(self, path):
        return path.startswith(self.sep)

    def isfile(self, path):
        return path in self.files and self.files[path] is not None

    def isdir(self, path):
        return self.normpath(path) in self.dirs

    def _slow_but_correct_join(self, *comps):
        return re.sub(re.escape(os.path.sep), self.sep, os.path.join(*comps))

    def join(self, *comps):
        # This function is called a lot, so we optimize it; there are
        # unit tests to check that we match _slow_but_correct_join(), above.
        path = ''
        sep = self.sep
        for comp in comps:
            if not comp:
                continue
            if comp[0] == sep:
                path = comp
                continue
            if path:
                path += sep
            path += comp
        if comps[-1] == '' and path:
            path += '/'
        path = path.replace(sep + sep, sep)
        return path

    def listdir(self, path):
        _, directories, files = list(self.walk(path))[0]
        return directories + files

    def walk(self, top):
        sep = self.sep
        if not self.isdir(top):
            raise OSError('%s is not a directory' % top)

        if not top.endswith(sep):
            top += sep

        directories = []
        files = []
        for file_path in self.files:
            if self.exists(file_path) and file_path.startswith(top):
                remaining = file_path[len(top):]
                if sep in remaining:
                    directory = remaining[:remaining.index(sep)]
                    if directory not in directories:
                        directories.append(directory)
                else:
                    files.append(remaining)
        file_system_tuples = [(top[:-1], directories, files)]
        for directory in directories:
            directory = top + directory
            tuples_from_subdirs = self.walk(directory)
            file_system_tuples += tuples_from_subdirs
        return file_system_tuples

    def mtime(self, path):
        if self.exists(path):
            return 0
        self._raise_not_found(path)

    def mktemp(self, suffix='', prefix='tmp', dir=None, **_):  # pylint: disable=redefined-builtin
        if dir is None:
            dir = self.sep + '__im_tmp'
        curno = self.current_tmpno
        self.current_tmpno += 1
        self.last_tmpdir = self.join(dir, '%s_%u_%s' % (prefix, curno, suffix))
        return self.last_tmpdir

    def mkdtemp(self, **kwargs):
        class TemporaryDirectory(object):
            def __init__(self, fs, **kwargs):
                self._kwargs = kwargs
                self._filesystem = fs
                self._directory_path = fs.mktemp(**kwargs)  # pylint: disable=protected-access
                fs.maybe_make_directory(self._directory_path)

            def __str__(self):
                return self._directory_path

            def __enter__(self):
                return self._directory_path

            def __exit__(self, exception_type, exception_value, traceback):
                # Only self-delete if necessary.

                # FIXME: Should we delete non-empty directories?
                if self._filesystem.exists(self._directory_path):
                    self._filesystem.rmtree(self._directory_path)

        return TemporaryDirectory(fs=self, **kwargs)

    def maybe_make_directory(self, *path):
        norm_path = self.normpath(self.join(*path))
        while norm_path and not self.isdir(norm_path):
            self.dirs.add(norm_path)
            norm_path = self.dirname(norm_path)

    def move(self, source, destination):
        if not self.exists(source):
            self._raise_not_found(source)
        if self.isfile(source):
            self.files[destination] = self.files[source]
            self.written_files[destination] = self.files[destination]
            self.files[source] = None
            self.written_files[source] = None
            return
        self.copytree(source, destination)
        self.rmtree(source)

    def _slow_but_correct_normpath(self, path):
        return re.sub(re.escape(os.path.sep), self.sep, os.path.normpath(path))

    def normpath(self, path):
        # This function is called a lot, so we try to optimize the common cases
        # instead of always calling _slow_but_correct_normpath(), above.
        if '..' in path or '/./' in path:
            # This doesn't happen very often; don't bother trying to optimize it.
            return self._slow_but_correct_normpath(path)
        if not path:
            return '.'
        if path == '/':
            return path
        if path == '/.':
            return '/'
        if path.endswith('/.'):
            return path[:-2]
        if path.endswith('/'):
            return path[:-1]
        return path

    def open_binary_tempfile(self, suffix=''):
        path = self.mktemp(suffix)
        return (WritableBinaryFileObject(self, path), path)

    def open_binary_file_for_reading(self, path):
        if self.files[path] is None:
            self._raise_not_found(path)
        return ReadableBinaryFileObject(self, path, self.files[path])

    def open_binary_file_for_writing(self, path):
        return WritableBinaryFileObject(self, path)

    def read_binary_file(self, path):
        # Intentionally raises KeyError if we don't recognize the path.
        if self.files[path] is None:
            self._raise_not_found(path)
        return self.files[path]

    def write_binary_file(self, path, contents):
        # FIXME: should this assert if dirname(path) doesn't exist?
        self.maybe_make_directory(self.dirname(path))
        self.files[path] = contents
        self.written_files[path] = contents

    def open_text_tempfile(self, suffix=''):
        path = self.mktemp(suffix)
        return (WritableTextFileObject(self, path), path)

    def open_text_file_for_reading(self, path):
        if self.files[path] is None:
            self._raise_not_found(path)
        contents = self.files[path].decode(_TEXT_ENCODING)
        return ReadableTextFileObject(self, path, contents)

    def open_text_file_for_writing(self, path):
        return WritableTextFileObject(self, path)

    def open_text_file_for_appending(self, path):
        return WritableTextFileObject(self, path, append=True)

    def read_text_file(self, path):
        return self.read_binary_file(path).decode(_TEXT_ENCODING)

    def write_text_file(self, path, contents):
        return self.write_binary_file(path, contents.encode(_TEXT_ENCODING))

    def sha1(self, path):
        contents = self.read_binary_file(path)
        return hashlib.sha1(contents).hexdigest()

    def relpath(self, path, start='.'):
        # Since os.path.relpath() calls os.path.normpath()
        # (see http://docs.python.org/library/os.path.html#os.path.abspath )
        # it also removes trailing slashes and converts forward and backward
        # slashes to the preferred slash os.sep.
        start = self.abspath(start)
        path = self.abspath(path)

        common_root = start
        dot_dot = ''
        while not common_root == '':
            if path.startswith(common_root):
                break
            common_root = self.dirname(common_root)
            dot_dot += '..' + self.sep

        rel_path = path[len(common_root):]

        if not rel_path:
            return '.'

        if rel_path[0] == self.sep:
            # It is probably sufficient to remove just the first character
            # since os.path.normpath() collapses separators, but we use
            # lstrip() just to be sure.
            rel_path = rel_path.lstrip(self.sep)
        elif not common_root == '/':
            # We are in the case typified by the following example:
            # path = "/tmp/foobar", start = "/tmp/foo" -> rel_path = "bar"
            common_root = self.dirname(common_root)
            dot_dot += '..' + self.sep
            rel_path = path[len(common_root) + 1:]

        return dot_dot + rel_path

    def remove(self, path, retry=True):
        if self.files[path] is None:
            self._raise_not_found(path)
        self.files[path] = None
        self.written_files[path] = None

    def rmtree(self, path_to_remove, ignore_errors=True, onerror=None):
        path_to_remove = self.normpath(path_to_remove)

        for file_path in self.files:
            # We need to add a trailing separator to path_to_remove to avoid matching
            # cases like path_to_remove='/foo/b' and file_path='/foo/bar/baz'.
            if file_path == path_to_remove or file_path.startswith(
                    path_to_remove + self.sep):
                self.files[file_path] = None

        def should_remove(directory):
            return directory == path_to_remove or directory.startswith(
                path_to_remove + self.sep)

        self.dirs = {d for d in self.dirs if not should_remove(d)}

    def remove_contents(self, dirname):
        return _remove_contents(self, dirname, sleep=lambda *args, **kw: None)

    def copytree(self, source, destination):
        source = self.normpath(source)
        destination = self.normpath(destination)

        for source_file in list(self.files):
            if source_file.startswith(source):
                destination_path = self.join(destination,
                                             self.relpath(source_file, source))
                self.maybe_make_directory(self.dirname(destination_path))
                self.files[destination_path] = self.files[source_file]

    def split(self, path):
        idx = path.rfind(self.sep)
        if idx == -1:
            return ('', path)
        return (path[:idx], path[(idx + 1):])

    def splitext(self, path):
        idx = path.rfind('.')
        if idx == -1:
            idx = len(path)
        return (path[0:idx], path[idx:])

    def symlink(self, source, link_name):
        raise NotImplementedError('Symlink not expected to be called in tests')

    def sanitize_filename(self, filename, replacement='_'):
        return _sanitize_filename(filename, replacement)


class WritableBinaryFileObject(object):
    def __init__(self, fs, path, append=False):
        self.fs = fs
        self.path = path
        self.closed = False
        if path not in self.fs.files or not append:
            self.setup_path(path)

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.close()

    def close(self):
        self.closed = True

    def write(self, string):
        self.fs.files[self.path] += string
        self.fs.written_files[self.path] = self.fs.files[self.path]

    def setup_path(self, path):
        self.fs.files[path] = b''


class WritableTextFileObject(WritableBinaryFileObject):
    def __init__(self, fs, path, append=False):
        super(WritableTextFileObject, self).__init__(fs, path, append)

    def write(self, string):
        contents = string.encode(_TEXT_ENCODING)
        super(WritableTextFileObject, self).write(contents)

    def writelines(self, lines):
        contents = b''.join(line.encode(_TEXT_ENCODING) for line in lines)
        self.fs.files[self.path] = contents
        self.fs.written_files[self.path] = contents

    def setup_path(self, path):
        super(WritableTextFileObject, self).setup_path(path)


class ReadableBinaryFileObject(object):
    def __init__(self, fs, path, data):
        self.fs = fs
        self.path = path
        self.closed = False
        self.data = data
        self.offset = 0
        try:
            # Maintain a text version if possible to
            # support readline.
            data_str = data.decode(_TEXT_ENCODING, 'replace')
            self.text = StringIO(data_str)
        except:
            pass

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.close()

    def close(self):
        self.closed = True

    def read(self, num_bytes=None):
        if not num_bytes:
            return self.data[self.offset:]
        start = self.offset
        self.offset += num_bytes
        return self.data[start:self.offset]

    def readline(self, length=None):
        return self.text.readline(length).encode(_TEXT_ENCODING, 'replace')

    def readlines(self):
        return self.text.readlines().encode(_TEXT_ENCODING, 'replace')

    def seek(self, offset, whence=os.SEEK_SET):
        if whence == os.SEEK_SET:
            self.offset = offset
        elif whence == os.SEEK_CUR:
            self.offset += offset
        elif whence == os.SEEK_END:
            self.offset = len(self.data) + offset
        else:
            assert False, "Unknown seek mode %s" % whence


class ReadableTextFileObject(ReadableBinaryFileObject):
    def __init__(self, fs, path, data):
        super(ReadableTextFileObject, self).__init__(fs, path, StringIO(data))

    def close(self):
        self.data.close()
        super(ReadableTextFileObject, self).close()

    def read(self, num_bytes=-1):
        return self.data.read(num_bytes)

    def readline(self, length=None):
        return self.data.readline(length)

    def readlines(self):
        return self.data.readlines()

    def __iter__(self):
        return self.data.__iter__()

    def next(self):
        return self.data.next()

    def seek(self, offset, whence=os.SEEK_SET):
        self.data.seek(offset, whence)


class FileSystemTestCase(unittest.TestCase):
    # pylint: disable=invalid-name
    # Use assertFilesAdded to be consistent with unittest.

    class _AssertFilesAddedContext(object):
        """Internal class used by FileTestCase.assertFilesAdded()."""

        def __init__(self, test_case, mock_filesystem, expected_files):
            self.test_case = test_case
            self.mock_filesystem = mock_filesystem
            self.expected_files = expected_files
            _ensure_binary_contents(self.expected_files)

        def __enter__(self):
            # Make sure that the expected_files aren't already in the mock
            # file system.
            for filepath in self.expected_files:
                assert filepath not in self.mock_filesystem.files, "%s was already in mock file system (%r)" % (
                    filepath, self.mock_filesystem.files)
            return self

        def __exit__(self, exc_type, exc_value, tb):
            # Exception already occurring, just exit.
            if exc_type is not None:
                return

            for filepath in sorted(self.expected_files):
                self.test_case.assertIn(filepath, self.mock_filesystem.files)
                self.test_case.assertEqual(
                    self.expected_files[filepath],
                    self.mock_filesystem.files[filepath])

    def assertFilesAdded(self, mock_filesystem, files):
        """Assert that the given files where added to the mock_filesystem.

        Use in a similar manner to self.assertRaises;

        with self.assertFilesAdded(mock_filesystem, {'/newfile': 'contents'}):
            code(mock_filesystem)
        """
        return self._AssertFilesAddedContext(self, mock_filesystem, files)
