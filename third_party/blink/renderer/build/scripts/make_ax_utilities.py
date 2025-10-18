#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator for ARIA utility functions from aria_properties.json5."""

import argparse
from typing import Callable, Dict, List, TypedDict

import json5_generator
import template_expander

from aria_properties import ARIAReader
from blinkbuild.name_style_converter import NameStyleConverter


class AttributeEntry(TypedDict):
    """Single ARIA attribute with its metadata."""

    name: str  # e.g. "aria-current"
    base_name: str  # e.g. "AriaCurrent"
    enum_values: List[str]  # e.g. ["page", "step", "date" ...]
    default_value: str  # e.g. "false" (empty string if no default)
    is_global: bool  # If True, supported_roles should be empty.
    supported_roles: List[str]  # e.g. ["button", "checkbox", ...]
    prevented_roles: List[str]  # e.g. ["caption", "code", ...] (empty if none)
    has_role_specific_values: bool  # True if any role has implicit value for this


class AttributeTypeInfo(TypedDict):
    """Information about a specific ARIA attribute type."""

    type_name: str  # e.g. "Boolean", "TokenList"
    attributes: List[AttributeEntry]  # All attributes of this type


class RoleEntry(TypedDict):
    """Single ARIA role mapping."""

    ariaRole: str  # e.g. "listbox"
    internalRole: str  # e.g. "kListBox"
    implicitValues: Dict[str, str]  # e.g. {"aria-orientation": "vertical"}
    nameFrom: List[
        str]  # e.g. ["author"], ["contents", "author"], ["prohibited"]


class MakeAxUtilitiesWriter(json5_generator.Writer):
    """Generator for ARIA utility functions."""

    def __init__(self, json5_file_path: str, output_dir: str) -> None:
        super().__init__(None, output_dir)
        self.aria_reader: ARIAReader = ARIAReader(json5_file_path)
        self._input_files: List[str] = [json5_file_path]

        # ARIA attributes categorized by type: "Boolean", "Token", etc.
        # For each type, the value is a list of attributes of that type.
        # For each attribute in the list, there is a base name and a list
        # of valid enum values (empty for non-token/token list types).
        self._attributes_by_type: Dict[str, AttributeTypeInfo] = {}

        # Non-deprecated ARIA roles: "button" -> `kButton`.
        self._current_roles: List[RoleEntry] = []

        # Deprecated ARIA roles: "directory" -> `kList`.
        self._deprecated_roles: List[RoleEntry] = []

        # Additional internal roles that map to the same ARIA role:
        # e.g. `kToggleButton` also maps to "button".
        self._additional_internal_role_mappings: List[RoleEntry] = []

        self._extract_roles()
        self._extract_attributes()

        self._outputs: Dict[str, Callable[[], str]] = {
            "ax_utilities_generated.h":
            self.generate_header,
            "ax_utilities_generated_attributes.cc":
            self.generate_implementation_attributes,
            "ax_utilities_generated_roles.cc":
            self.generate_implementation_roles,
            "ax_utilities_generated_naming.cc":
            self.generate_implementation_naming,
        }

        header = self._relative_output_dir + "ax_utilities_generated.h"

        self._template_context = {
            "input_files": self._input_files,
            "header_guard": self.make_header_guard(header),
            "attributes_by_type": self._attributes_by_type,
            "current_roles": self._current_roles,
            "deprecated_roles": self._deprecated_roles,
            "additional_internal_role_mappings":
            self._additional_internal_role_mappings,
            "this_include_path": header,
        }

    def _extract_attributes(self) -> None:
        """Extract and categorize all ARIA attributes for utility generation.

        Used to generate:
        - Type-specific getters: `GetAriaBooleanAttrs()`, `GetAriaTokenAttrs()`
        - Type checkers: `IsAriaBooleanAttribute()`
        - Value getters: `GetAriaHiddenValues()` (for token/token list types)
        - General value lookup: `GetValidValuesForAriaAttribute()`
        - Role support functions: `RoleSupportsAriaFoo()`
        """

        # First, find all attributes that have role-specific implicit values
        attrs_with_role_values = set()
        for role in self._current_roles + self._deprecated_roles:
            for attr_name in role["implicitValues"].keys():
                attrs_with_role_values.add(attr_name)

        attributes_by_type: Dict[str, AttributeTypeInfo] = {}

        for attr in self.aria_reader.attributes():
            # Skip aria-virtualcontent: This is not in the ARIA specification,
            # nor has a recent draft entry for the ARIA specification been
            # located for this proposed attribute.
            if attr["name"] == "aria-virtualcontent":
                continue

            # Use same logic as `make_qualified_names.py` so that any generated
            # functions which include ARIA attributes use the correct qualified
            # name. This base name will also be used to generate functions like
            # `GetAriaCurrentValues()` for token and token list attributes.
            base_name = NameStyleConverter(attr["name"]).to_upper_camel_case()

            attr_type = attr.get("type")
            if attr_type not in attributes_by_type:
                type_name_overrides: Dict[str, str] = {
                    "IDREF": "Idref",
                    "IDREF_list": "IdrefList",
                    "token_list": "TokenList",
                }
                type_name = type_name_overrides.get(
                    attr_type,
                    NameStyleConverter(attr_type).to_upper_camel_case())

                attributes_by_type[attr_type] = {
                    "type_name": type_name,
                    "attributes": [],
                }

            # Always set `enum_values`, even for non-token/token list types,
            # so that we can send one attributes dictionary to the template.
            is_global = attr.get("isGlobal", False)
            supported_roles = attr.get("supportedOnRoles", [])
            prevented_roles = attr.get("preventedOnRoles", [])

            # Enforce data integrity:
            # - Non-global attributes MUST have "supportedOnRoles".
            # - Global attributes MUST NOT have "supportedOnRoles".
            # - Both can have "preventedOnRoles" (optional).
            if is_global:
                assert not supported_roles, (
                    f"Global attribute '{attr['name']}' must not have "
                    f"supportedOnRoles")
            else:
                assert supported_roles, (
                    f"Non-global attribute '{attr['name']}' must have "
                    f"supportedOnRoles")

            # Get default value and convert booleans to lowercase strings
            default_value = attr.get("default", "")
            if isinstance(default_value, bool):
                default_value = "true" if default_value else "false"
            else:
                default_value = str(default_value) if default_value else ""

            attributes_by_type[attr_type]["attributes"].append({
                "name":
                attr["name"],
                "base_name":
                base_name,
                "enum_values":
                attr.get("enum", []),
                "default_value":
                default_value,
                "supported_roles":
                supported_roles,
                "prevented_roles":
                prevented_roles,
                "is_global":
                is_global,
                "has_role_specific_values":
                attr["name"] in attrs_with_role_values,
            })

        self._attributes_by_type = attributes_by_type

    def _extract_roles(self) -> None:
        """Extract and categorize concrete ARIA roles for utility generation.

        Used to generate:
        - `AriaRoleToInternalRole()`
        - `InternalRoleToAriaRole()` (prevents deprecated roles from being used)
        - `GetAriaRoleNames()` (all non-deprecated roles, e.g. for fuzz testing)
        - Static lookup tables to support the above
        """

        current_roles: List[RoleEntry] = []
        additional_internal_role_mappings: List[RoleEntry] = []
        deprecated_roles: List[RoleEntry] = []

        for role in self.aria_reader.roles():
            if role.get("abstract", False):
                continue

            internal_roles: List[str] = role.get(
                "internalRoles", [f"k{role['name'].capitalize()}"])

            # Convert implicit values to strings for consistency
            implicit_values = {}
            for attr_name, value in role.get("implicitValues", {}).items():
                # Convert booleans to lowercase strings ("true"/"false")
                if isinstance(value, bool):
                    implicit_values[attr_name] = "true" if value else "false"
                else:
                    implicit_values[attr_name] = str(value)

            name_from = role.get("nameFrom", [])

            entry: RoleEntry = {
                "ariaRole": role["name"],
                "internalRole": internal_roles[0],
                "implicitValues": implicit_values,
                "nameFrom": name_from,
            }
            if role.get("deprecated"):
                deprecated_roles.append(entry)
            else:
                current_roles.append(entry)

            for internal_role in internal_roles[1:]:
                additional_entry: RoleEntry = {
                    "ariaRole": role["name"],
                    "internalRole": internal_role,
                    "implicitValues": implicit_values,
                    "nameFrom": name_from,
                }
                additional_internal_role_mappings.append(additional_entry)

        self._current_roles = sorted(current_roles,
                                     key=lambda x: x["ariaRole"])
        self._deprecated_roles = sorted(deprecated_roles,
                                        key=lambda x: x["ariaRole"])
        self._additional_internal_role_mappings = sorted(
            additional_internal_role_mappings, key=lambda x: x["ariaRole"])

    @template_expander.use_jinja("templates/ax_utilities_generated.h.tmpl")
    def generate_header(self) -> dict:
        """Generate the public header file with ARIA utility function declarations."""

        return self._template_context

    @template_expander.use_jinja(
        "templates/ax_utilities_generated_attributes.cc.tmpl")
    def generate_implementation_attributes(self) -> dict:
        """Generate the implementation file with ARIA attribute utility definitions."""

        return self._template_context

    @template_expander.use_jinja(
        "templates/ax_utilities_generated_roles.cc.tmpl")
    def generate_implementation_roles(self) -> dict:
        """Generate the implementation file with ARIA role mapping utilities."""

        return self._template_context

    @template_expander.use_jinja(
        "templates/ax_utilities_generated_naming.cc.tmpl")
    def generate_implementation_naming(self) -> dict:
        """Generate the implementation file with ARIA naming support utilities."""

        return self._template_context


def main():
    """Main function to generate ARIA utilities from command line."""

    parser = argparse.ArgumentParser()
    parser.add_argument("json5_file", help="Path to aria_properties.json5")
    parser.add_argument("--output_dir", required=True, help="Output directory")
    args = parser.parse_args()

    writer = MakeAxUtilitiesWriter(args.json5_file, args.output_dir)
    writer.write_files(args.output_dir)


if __name__ == "__main__":
    main()
