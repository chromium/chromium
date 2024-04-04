# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


class LRUMapping(collections.abc.MutableMapping):
    """A mapping with at most N most recently used entries. Not threadsafe."""

    def __init__(self, capacity: int):
        # Most recently used entries are first.
        self._inner = collections.OrderedDict()
        self.capacity = capacity

    @property
    def capacity(self) -> int:
        return self._capacity

    @capacity.setter
    def capacity(self, n: int):
        if n < 0:
            raise ValueError(f'capacity must be nonnegative, but was {n}')
        self._capacity = n

    def __getitem__(self, key):
        self._inner.move_to_end(key, last=False)
        return self._inner[key]

    def __setitem__(self, key, value):
        self._inner[key] = value
        self._inner.move_to_end(key, last=False)
        self._maybe_truncate()

    def __delitem__(self, key):
        del self._inner[key]

    def __iter__(self):
        return iter(self._inner)

    def __len__(self) -> int:
        return len(self._inner)

    def _maybe_truncate(self):
        while len(self._inner) > self._capacity:
            self._inner.popitem()
