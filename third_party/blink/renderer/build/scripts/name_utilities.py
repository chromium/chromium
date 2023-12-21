# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

from blinkbuild.name_style_converter import NameStyleConverter


def cpp_bool(value):
    if value is True:
        return 'true'
    if value is False:
        return 'false'
    # Return value as is, which for example may be a platform-dependent constant
    # such as "defaultSelectTrailingWhitespaceEnabled".
    return value


def cpp_name(entry):
    return entry['ImplementedAs'] or entry['name'].original


def enum_key_for_css_keyword(keyword):
    # To make sure different enum keys are generated for infinity and -infinity.
    # Design doc : https://bit.ly/349gXjq
    if (not isinstance(keyword, str)) and keyword.original == '-infinity':
        return 'kNegative' + _upper_camel_case(keyword)
    return 'k' + _upper_camel_case(keyword)


def enum_key_for_css_property(property_name):
    return 'k' + _upper_camel_case(property_name)


def enum_key_for_css_property_alias(property_name):
    return 'kAlias' + property_name.to_upper_camel_case()


# This id is used to build function names returning CSS properties (e.g.
# GetCSSPropertyX(), GetCSSPropertyXInternal(), etc.)
def id_for_css_property(property_name):
    return 'CSSProperty' + _upper_camel_case(property_name)


def id_for_css_property_alias(property_name):
    return 'CSSPropertyAlias' + property_name.to_upper_camel_case()


def _upper_camel_case(property_name):
    converter = NameStyleConverter(property_name) if isinstance(
        property_name, str) else property_name
    return converter.to_upper_camel_case()


def tag_symbol_for_entry(json_entry):
    suffix = "OrUnknown" if json_entry.get('runtimeFlagHasOriginTrial',
                                           False) else ""
    return 'k' + NameStyleConverter(
        json_entry['name'].original).to_upper_camel_case() + suffix
