# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Optional, Sequence, Set, Union
import dataclasses


@dataclasses.dataclass(frozen=True)
class IncludeDefinition:
    """Definition for an #include statement."""
    filename: str  # Header filename to include.
    annotation: Optional[str] = None  # End-of-line comment (e.g. IWYU pragma).


class CodeGenAccumulator(object):
    """
    Accumulates a variety of information and helps generate code based on the
    information.
    """

    def __init__(self):
        # Headers of non-standard library to be included
        self._include_headers = set()  # type: Set[IncludeDefinition]
        # Headers of C++ standard library to be included
        self._stdcpp_include_headers = set()
        # Forward declarations of C++ class
        self._class_decls = set()
        # Forward declarations of C++ struct
        self._struct_decls = set()

    def total_size(self):
        return (len(self.include_headers) + len(self.class_decls) +
                len(self.struct_decls) + len(self.stdcpp_include_headers))

    @property
    def include_headers(self) -> Set[IncludeDefinition]:
        return self._include_headers

    def add_include_headers(self, headers: Sequence[Union[str,
                                                          IncludeDefinition]]):
        """Add a list of headers to include. Individual headers can be specified
        either as a filename string, or as an IncludeDefinition instance (useful
        to add IWYU pragma annotations)."""
        self._include_headers.update(
            IncludeDefinition(header) if isinstance(header, str) else header
            for header in headers if header)

    @staticmethod
    def require_include_headers(headers: Sequence[Union[str,
                                                        IncludeDefinition]]):
        return lambda accumulator: accumulator.add_include_headers(headers)

    @property
    def stdcpp_include_headers(self) -> Set[IncludeDefinition]:
        return self._stdcpp_include_headers

    def add_stdcpp_include_headers(
            self, headers: Sequence[Union[str, IncludeDefinition]]):
        """Add a list of standard headers to include. Individual headers can
        be specified either as a filename string, or as an IncludeDefinition
        instance (useful to add IWYU pragma annotations)."""
        self._stdcpp_include_headers.update(
            IncludeDefinition(header) if isinstance(header, str) else header
            for header in headers if header)

    @staticmethod
    def require_stdcpp_include_headers(
            headers: Sequence[Union[str, IncludeDefinition]]):
        return lambda accumulator: accumulator.add_stdcpp_include_headers(
            headers)

    @property
    def class_decls(self):
        return self._class_decls

    def add_class_decls(self, class_names):
        self._class_decls.update(filter(None, class_names))

    @staticmethod
    def require_class_decls(class_names):
        return lambda accumulator: accumulator.add_class_decls(class_names)

    @property
    def struct_decls(self):
        return self._struct_decls

    def add_struct_decls(self, struct_names):
        self._struct_decls.update(filter(None, struct_names))

    @staticmethod
    def require_struct_decls(struct_names):
        return lambda accumulator: accumulator.add_struct_decls(struct_names)
