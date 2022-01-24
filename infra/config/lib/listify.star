# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def listify(*args):
    """Create a single list from multiple arguments.

    Each argument can be either a single element or a list of elements. A single
    element will appear as an element in the resulting list iff it is non-None.
    A list of elements will have all non-None elements appear in the resulting
    list.
    """
    l = []
    for a in args:
        if type(a) != type([]):
            a = [a]
        for e in a:
            if e != None:
                l.append(e)
    return l
