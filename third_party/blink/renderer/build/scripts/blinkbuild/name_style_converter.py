# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

import copy
import re

SPECIAL_TOKENS = [
    # This list should be sorted by length.
    'WebSocket',
    'String16',
    'Float32',
    'Float64',
    'Base64',
    'IFrame',
    'Latin1',
    'MathML',
    'PlugIn',
    'SQLite',
    'Uint16',
    'Uint32',
    'WebGL2',
    'webgl2',
    'WebGPU',
    'ASCII',
    'CSSOM',
    'CType',
    'DList',
    'Int16',
    'Int32',
    'MPath',
    'OList',
    'TSpan',
    'UList',
    'UTF16',
    'Uint8',
    'WebGL',
    'XPath',
    'ETC1',
    'etc1',
    'HTML',
    'Int8',
    'S3TC',
    's3tc',
    'SPv2',
    'UTF8',
    'sRGB',
    'URLs',
    'API',
    'CSS',
    'DNS',
    'DOM',
    'EXT',
    'RTC',
    'SVG',
    'XSS',
    '2D',
    'AX',
    'FE',
    'V0',
    'V8',
    'v8',
]

_SPECIAL_TOKENS_WITH_NUMBERS = [token for token in SPECIAL_TOKENS if re.search(r'[0-9]', token)]

# Applying _TOKEN_PATTERNS repeatedly should capture any sequence of a-z, A-Z,
# 0-9.
_TOKEN_PATTERNS = [
    # 'Foo' 'foo'
    '[A-Z]?[a-z]+',
    # The following pattern captures only 'FOO' in 'FOOElement'.
    '[A-Z]+(?![a-z])',
    # '2D' '3D', but not '2Dimension'
    '[0-9][Dd](?![a-z])',
    '[0-9]+',
]

_TOKEN_RE = re.compile(r'(' + '|'.join(SPECIAL_TOKENS + _TOKEN_PATTERNS) + r')')


def tokenize_name(name):
    """Tokenize the specified name.

    A token consists of A-Z, a-z, and 0-9 characters. Other characters work as
    token delimiters, and the resultant list won't contain such characters.
    Capital letters also work as delimiters.  E.g. 'FooBar-baz' is tokenized to
    ['Foo', 'Bar', 'baz']. See _TOKEN_PATTERNS for more details.

    This function detects special cases that are not easily discernible without
    additional knowledge, such as recognizing that in SVGSVGElement, the first
    two SVGs are separate tokens, but WebGL is one token.

    Returns:
        A list of token strings.

    """

    # In case |name| is written in lowerCamelCase, we try to match special
    # tokens that contains numbers ignoring cases only at the first step.
    tokens = []
    match = re.search(r'^(' + '|'.join(_SPECIAL_TOKENS_WITH_NUMBERS) + r')', name, re.IGNORECASE)
    if match:
        tokens.append(match.group(0))
        name = name[match.end(0):]

    return tokens + _TOKEN_RE.findall(name)


class NameStyleConverter(object):
    """Converts names from camelCase to various other styles.
    """

    def __init__(self, name):
        self.tokens = tokenize_name(name)
        self._original = name

    @property
    def original(self):
        return self._original

    def __str__(self):
        return self._original

    # Make this class workable with sort().
    def __lt__(self, other):
        return self.original < other.original

    # Make this class workable with groupby().
    def __eq__(self, other):
        return self.original == other.original

    def to_snake_case(self):
        """Snake case is the file and variable name style per Google C++ Style
           Guide:
           https://google.github.io/styleguide/cppguide.html#Variable_Names

           Also known as the hacker case.
           https://en.wikipedia.org/wiki/Snake_case
        """
        return '_'.join([token.lower() for token in self.tokens])

    def to_upper_camel_case(self):
        """Upper-camel case is the class and function name style per
           Google C++ Style Guide:
           https://google.github.io/styleguide/cppguide.html#Function_Names

           Also known as the PascalCase.
           https://en.wikipedia.org/wiki/Camel_case.
        """
        tokens = self.tokens
        # If the first token is one of SPECIAL_TOKENS, we should replace the
        # token with the matched special token.
        # e.g. ['css', 'External', 'Scanner', 'Preload'] => 'CSSExternalScannerPreload'
        if tokens and tokens[0].lower() == tokens[0]:
            for special in SPECIAL_TOKENS:
                if special.lower() == tokens[0]:
                    tokens = copy.deepcopy(tokens)
                    tokens[0] = special
                    break
        return ''.join([token[0].upper() + token[1:] for token in tokens])

    def to_lower_camel_case(self):
        """Lower camel case is the name style for attribute names and operation
           names in web platform APIs.
           e.g. 'addEventListener', 'documentURI', 'fftSize'
           https://en.wikipedia.org/wiki/Camel_case.
        """
        if not self.tokens:
            return ''
        return self.tokens[0].lower() + ''.join([token[0].upper() + token[1:] for token in self.tokens[1:]])

    def to_macro_case(self):
        """Macro case is the macro name style per Google C++ Style Guide:
           https://google.github.io/styleguide/cppguide.html#Macro_Names
        """
        return '_'.join([token.upper() for token in self.tokens])

    def to_all_cases(self):
        return {
            'snake_case': self.to_snake_case(),
            'upper_camel_case': self.to_upper_camel_case(),
            'macro_case': self.to_macro_case(),
        }

    # Use the following high level naming functions which describe the semantics
    # of the name, rather than a particular style.

    def to_class_name(self, prefix=None, suffix=None):
        """Represents this name as a class name in Chromium C++ style.

        i.e. UpperCamelCase.
        """
        camel_prefix = prefix[0].upper() + prefix[1:].lower() if prefix else ''
        camel_suffix = suffix[0].upper() + suffix[1:].lower() if suffix else ''
        return camel_prefix + self.to_upper_camel_case() + camel_suffix

    def to_class_data_member(self, prefix=None, suffix=None):
        """Represents this name as a data member name in Chromium C++ style.

        i.e. snake_case_with_trailing_underscore_.
        """
        lower_prefix = prefix.lower() + '_' if prefix else ''
        lower_suffix = suffix.lower() + '_' if suffix else ''
        return lower_prefix + self.to_snake_case() + '_' + lower_suffix

    def to_function_name(self, prefix=None, suffix=None):
        """Represents this name as a function name in Blink C++ style.

        i.e. UpperCamelCase
        Note that this function should not be used for IDL operation names and
        C++ functions implementing IDL operations and attributes.
        """
        camel_prefix = prefix[0].upper() + prefix[1:].lower() if prefix else ''
        camel_suffix = ''
        if type(suffix) is list:
            for item in suffix:
                camel_suffix += item[0].upper() + item[1:].lower()
        elif suffix:
            camel_suffix = suffix[0].upper() + suffix[1:].lower()
        return camel_prefix + self.to_upper_camel_case() + camel_suffix

    def to_enum_value(self):
        """Represents this name as an enum value in Blink C++ style.

        i.e. kUpperCamelCase
        """
        return 'k' + self.to_upper_camel_case()

    def to_header_guard(self):
        """Represents this name as a header guard style in Chromium C++ style.

        i.e. THIRD_PARTY_BLINK_RENDERER_MODULES_MODULES_EXPORT_H_
        """
        return re.sub(r'[-/.]', '_', self.to_macro_case()) + '_'
