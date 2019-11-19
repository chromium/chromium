# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pickle


def read_pickle_file(filepath):
    """
    Reads the content of the file as a pickled object.
    """
    with open(filepath, 'r') as file_obj:
        return pickle.load(file_obj)


def write_pickle_file_if_changed(filepath, obj):
    """
    Writes the given object out to |filepath| if the content changed.

    Returns True if the object is written to the file, and False if skipped.
    """
    return write_to_file_if_changed(filepath, pickle.dumps(obj))


def write_to_file_if_changed(filepath, contents):
    """
    Writes the given contents out to |filepath| if the contents changed.

    Returns True if the data is written to the file, and False if skipped.
    """
    try:
        with open(filepath, 'r') as file_obj:
            old_contents = file_obj.read()
    except (OSError, EnvironmentError):
        pass
    else:
        if contents == old_contents:
            return False
        os.remove(filepath)
    with open(filepath, 'w') as file_obj:
        file_obj.write(contents)
    return True
