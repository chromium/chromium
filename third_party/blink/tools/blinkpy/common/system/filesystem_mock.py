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

import contextlib
import errno
import hashlib
import io
import os
import re
import unittest
from unittest.mock import patch

import six

from blinkpy.common.system.filesystem import _remove_contents, _sanitize_filename

_TEXT_ENCODING = 'utf-8'


def _ensure_binary_contents(file_contents):
    # Iterate over a copy while the underlying mapping is mutated.
    for path, contents in list(file_contents.items()):
        if contents is not None:
            contents = six.ensure_binary(contents, _TEXT_ENCODING)
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
        raise FileNotFoundError('%s: %s' % (os.strerror(errno.ENOENT), path))

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
        # Allow zero directories (e.g., `a/**/b` matches `a/b`).
        glob_string = glob_string.replace('/\\*\\*', '(|/.*)')
        glob_string = glob_string.replace('\\*', '[^\\/]*')
        glob_string = glob_string.replace('\\/', '/')
        path_filter = lambda path: re.fullmatch(glob_string, path)

        # We could use fnmatch.fnmatch, but that might not do the right thing on Windows.
        existing_files = [
            path for path, contents in self.files.items()
            if contents is not None
        ]
        yield from filter(path_filter, existing_files)
        yield from filter(path_filter, self.dirs)

    def isabs(self, path):
        return path.startswith(self.sep)

    def isfile(self, path):
        return path in self.files and self.files[path] is not None

    def isdir(self, path):
        return self.normpath(path) in self.dirs

    def _slow_but_correct_join(self, comp, *comps):
        return re.sub(re.escape(os.path.sep), self.sep,
                      os.path.join(comp, *comps))

    def join(self, *comps):
        # The real `os.path.join` accepts both strings and bytes:
        #   (*bytes) -> bytes
        #   (*str) -> str
        # Record what type the caller originally passed, perform the join with
        # text strings, then coerce the return value to the original argument
        # type.
        binary_mode = all(isinstance(comp, bytes) for comp in comps)
        # This function is called a lot, so we optimize it; there are
        # unit tests to check that we match _slow_but_correct_join(), above.
        path = ''
        sep = self.sep
        for comp in comps:
            if not comp:
                continue
            comp = six.ensure_text(comp)
            if comp[0] == sep:
                path = comp
                continue
            if path:
                path += sep
            path += comp
        if six.ensure_text(comps[-1]) == '' and path:
            path += '/'
        path = path.replace(sep + sep, sep)
        return path.encode() if binary_mode else path

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
        # The real `os.walk(...)` [0] gives the caller a chance to modify which
        # subdirectories to traverse by mutating the `directories` list, so we
        # should yield here instead of returning a precomputed list.
        #
        # [0]: https://docs.python.org/3/library/os.html#os.walk
        yield (top[:-1], directories, files)
        for directory in directories:
            directory = top + directory
            yield from self.walk(directory)

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
        return self.open_binary_file_for_writing(path), path

    def open_binary_file_for_reading(self, path):
        if self.files.get(path) is None:
            self._raise_not_found(path)
        return BufferedReader(WriteThroughBinaryFile(self, path))

    def open_binary_file_for_writing(self, path):
        self.files[path] = b''
        return WriteThroughBinaryFile(self, path)

    def read_binary_file(self, path):
        maybe_contents = self.files.get(path)
        if maybe_contents is None:
            self._raise_not_found(path)
        return maybe_contents

    def write_binary_file(self, path, contents):
        # FIXME: should this assert if dirname(path) doesn't exist?
        self.maybe_make_directory(self.dirname(path))
        self.files[path] = contents
        self.written_files[path] = contents

    def open_text_tempfile(self, suffix=''):
        path = self.mktemp(suffix)
        return self.open_text_file_for_writing(path), path

    def open_text_file_for_reading(self, path):
        return TextIOWrapper(self.open_binary_file_for_reading(path))

    def open_text_file_for_writing(self, path):
        return TextIOWrapper(self.open_binary_file_for_writing(path))

    def open_text_file_for_appending(self, path):
        self.files.setdefault(path, b'')
        file_handle = TextIOWrapper(WriteThroughBinaryFile(self, path))
        file_handle.seek(0, io.SEEK_END)
        return file_handle

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
        if self.files.get(path) is None:
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

    def _open_mock(self, filename, mode='r', **_kwargs):
        """A mock for Python's built-in `open` backed by this Blink FS."""
        mode_match = re.match(r'([rwa])(b?)', mode)
        open_func_map = {
            ('r', ''): self.open_text_file_for_reading,
            ('w', ''): self.open_text_file_for_writing,
            ('r', 'b'): self.open_binary_file_for_reading,
            ('w', 'b'): self.open_binary_file_for_writing,
        }
        return open_func_map[mode_match.groups()](filename)

    @contextlib.contextmanager
    def patch_builtins(self):
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch('builtins.open', self._open_mock))
            stack.enter_context(patch('os.sep', self.sep))
            stack.enter_context(patch('os.path.sep', self.sep))
            stack.enter_context(patch('os.path.abspath', self.abspath))
            stack.enter_context(patch('os.path.relpath', self.relpath))
            stack.enter_context(patch('os.path.join', self.join))
            stack.enter_context(patch('os.path.isfile', self.isfile))
            stack.enter_context(patch('os.path.isdir', self.isdir))
            stack.enter_context(patch('os.path.exists', self.exists))
            stack.enter_context(patch('os.makedirs',
                                      self.maybe_make_directory))
            stack.enter_context(patch('os.replace', self.move))
            stack.enter_context(patch('os.unlink', self.remove))
            stack.enter_context(
                patch('tempfile.TemporaryFile',
                      lambda *args, **kwargs: self.open_text_tempfile()[0]))
            stack.enter_context(
                patch('tempfile.NamedTemporaryFile',
                      lambda *args, **kwargs: self.open_text_tempfile()[0]))
            yield


class BufferedReader(io.BufferedReader):
    def __init__(self, raw, **options):
        super().__init__(raw, **options)
        self.fs = raw.fs


class TextIOWrapper(io.TextIOWrapper):
    def __init__(self,
                 raw,
                 encoding=_TEXT_ENCODING,
                 errors='replace',
                 newline='\n',
                 **options):
        super().__init__(raw,
                         encoding=encoding,
                         errors=errors,
                         newline=newline,
                         **options)
        self.fs = raw.fs


class WriteThroughBinaryFile(io.BytesIO):
    def __init__(self, fs, name: str):
        self.fs = fs
        self.name = name
        super().__init__(self.fs.files[self.name])

    def write(self, buf):
        amount_written = super().write(buf)
        self.fs.files[self.name] += buf
        self.fs.written_files[self.name] = self.fs.files[self.name]
        return amount_written

    def writelines(self, lines):
        super().writelines(lines)
        contents = b''.join(lines)
        self.fs.files[self.name] = contents
        self.fs.written_files[self.name] = contents

    def truncate(self, size=None):
        new_size = super().truncate(size)
        self.fs.files[self.name] = self.getvalue()
        return new_size


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
