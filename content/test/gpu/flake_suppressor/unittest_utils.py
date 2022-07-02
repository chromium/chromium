# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pyfakefs import fake_filesystem_unittest  # pylint: disable=import-error


def CreateFile(test: fake_filesystem_unittest.TestCase, *args,
               **kwargs) -> None:
  # TODO(crbug.com/1156806): Remove this and just use fs.create_file() when
  # Catapult is updated to a newer version of pyfakefs that is compatible with
  # Chromium's version.
  if hasattr(test.fs, 'create_file'):
    test.fs.create_file(*args, **kwargs)
  else:
    test.fs.CreateFile(*args, **kwargs)


class FakeProcess():
  def __init__(self, stdout: str):
    self.stdout = stdout or ''
