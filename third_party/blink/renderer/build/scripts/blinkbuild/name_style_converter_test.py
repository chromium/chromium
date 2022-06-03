# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import,protected-access
"""Unit tests for name_style_converter.py."""

import unittest

from name_style_converter import NameStyleConverter
from name_style_converter import tokenize_name


class SmartTokenizerTest(unittest.TestCase):
    def test_simple_cases(self):
        self.assertEqual(tokenize_name('foo'), ['foo'])

        self.assertEqual(tokenize_name('fooBar'), ['foo', 'Bar'])

        self.assertEqual(tokenize_name('fooBarBaz'), ['foo', 'Bar', 'Baz'])

        self.assertEqual(tokenize_name('Baz'), ['Baz'])

        self.assertEqual(tokenize_name(''), [])

        self.assertEqual(tokenize_name('FOO'), ['FOO'])

        self.assertEqual(tokenize_name('foo2'), ['foo', '2'])

    def test_tricky_cases(self):
        self.assertEqual(
            tokenize_name('XMLHttpRequest'), ['XML', 'Http', 'Request'])

        self.assertEqual(tokenize_name('HTMLElement'), ['HTML', 'Element'])

        self.assertEqual(
            tokenize_name('WebGLRenderingContext'),
            ['WebGL', 'Rendering', 'Context'])

        self.assertEqual(
            tokenize_name('CanvasRenderingContext2D'),
            ['Canvas', 'Rendering', 'Context', '2D'])
        self.assertEqual(
            tokenize_name('CanvasRenderingContext2DAPITest'),
            ['Canvas', 'Rendering', 'Context', '2D', 'API', 'Test'])

        self.assertEqual(
            tokenize_name('SVGSVGElement'), ['SVG', 'SVG', 'Element'])

        self.assertEqual(
            tokenize_name('CanvasRenderingContext2D'),
            ['Canvas', 'Rendering', 'Context', '2D'])

        self.assertEqual(
            tokenize_name('CSSURLImageValue'),
            ['CSS', 'URL', 'Image', 'Value'])
        self.assertEqual(
            tokenize_name('CSSPropertyAPID'), ['CSS', 'Property', 'API', 'D'])
        self.assertEqual(
            tokenize_name('AXARIAGridCell'), ['AX', 'ARIA', 'Grid', 'Cell'])

        self.assertEqual(tokenize_name('CDATASection'), ['CDATA', 'Section'])

        self.assertEqual(tokenize_name('ASCIICType'), ['ASCII', 'CType'])

        self.assertEqual(
            tokenize_name('HTMLDListElement'), ['HTML', 'DList', 'Element'])
        self.assertEqual(
            tokenize_name('HTMLOListElement'), ['HTML', 'OList', 'Element'])
        self.assertEqual(
            tokenize_name('HTMLIFrameElement'), ['HTML', 'IFrame', 'Element'])
        self.assertEqual(
            tokenize_name('HTMLPlugInElement'), ['HTML', 'PlugIn', 'Element'])

        # No special handling for OptGroup, FieldSet, and TextArea.
        self.assertEqual(
            tokenize_name('HTMLOptGroupElement'),
            ['HTML', 'Opt', 'Group', 'Element'])
        self.assertEqual(
            tokenize_name('HTMLFieldSetElement'),
            ['HTML', 'Field', 'Set', 'Element'])
        self.assertEqual(
            tokenize_name('HTMLTextAreaElement'),
            ['HTML', 'Text', 'Area', 'Element'])

        self.assertEqual(tokenize_name('Path2D'), ['Path', '2D'])
        self.assertEqual(tokenize_name('Point2D'), ['Point', '2D'])
        self.assertEqual(
            tokenize_name('CanvasRenderingContext2DState'),
            ['Canvas', 'Rendering', 'Context', '2D', 'State'])
        self.assertEqual(
            tokenize_name('Accelerated2dCanvas'),
            ['Accelerated', '2d', 'Canvas'])

        self.assertEqual(
            tokenize_name('RTCDTMFSender'), ['RTC', 'DTMF', 'Sender'])

        self.assertEqual(
            tokenize_name('WebGLCompressedTextureS3TCsRGB'),
            ['WebGL', 'Compressed', 'Texture', 'S3TC', 'sRGB'])
        self.assertEqual(
            tokenize_name('WebGL2CompressedTextureETC1'),
            ['WebGL2', 'Compressed', 'Texture', 'ETC1'])
        self.assertEqual(tokenize_name('EXTsRGB'), ['EXT', 'sRGB'])
        # 'PVRTC' contains a special token 'RTC', but it should be a
        # single token.
        self.assertEqual(
            tokenize_name('WebGLCompressedTexturePVRTC'),
            ['WebGL', 'Compressed', 'Texture', 'PVRTC'])

        self.assertEqual(
            tokenize_name('SVGFEBlendElement'),
            ['SVG', 'FE', 'Blend', 'Element'])
        self.assertEqual(
            tokenize_name('SVGMPathElement'), ['SVG', 'MPath', 'Element'])
        self.assertEqual(
            tokenize_name('SVGTSpanElement'), ['SVG', 'TSpan', 'Element'])
        self.assertEqual(
            tokenize_name('SVGURIReference'), ['SVG', 'URI', 'Reference'])

        self.assertEqual(
            tokenize_name('UTF16TextIterator'), ['UTF16', 'Text', 'Iterator'])
        self.assertEqual(tokenize_name('UTF8Decoder'), ['UTF8', 'Decoder'])
        self.assertEqual(tokenize_name('Uint8Array'), ['Uint8', 'Array'])
        self.assertEqual(
            tokenize_name('DOMWindowBase64'), ['DOM', 'Window', 'Base64'])
        self.assertEqual(
            tokenize_name('TextCodecLatin1'), ['Text', 'Codec', 'Latin1'])
        self.assertEqual(
            tokenize_name('V8BindingForCore'),
            ['V8', 'Binding', 'For', 'Core'])
        self.assertEqual(tokenize_name('V8DOMRect'), ['V8', 'DOM', 'Rect'])
        self.assertEqual(
            tokenize_name('String16MojomTraits'),
            ['String16', 'Mojom', 'Traits'])

        self.assertEqual(
            tokenize_name('V0InsertionPoint'), ['V0', 'Insertion', 'Point'])
        self.assertEqual(
            tokenize_name('ShadowDOMV0Test'), ['Shadow', 'DOM', 'V0', 'Test'])
        self.assertEqual(
            tokenize_name('ElementShadowV0'), ['Element', 'Shadow', 'V0'])
        self.assertEqual(
            tokenize_name('StubChromeClientForSPv2'),
            ['Stub', 'Chrome', 'Client', 'For', 'SPv2'])

        self.assertEqual(
            tokenize_name('SQLiteAuthorizer'), ['SQLite', 'Authorizer'])
        self.assertEqual(
            tokenize_name('XPathEvaluator'), ['XPath', 'Evaluator'])

        self.assertEqual(
            tokenize_name('IsXHTMLDocument'), ['Is', 'XHTML', 'Document'])
        self.assertEqual(
            tokenize_name('isHTMLDocument'), ['is', 'HTML', 'Document'])

        self.assertEqual(tokenize_name('matrix3d'), ['matrix', '3d'])

        self.assertEqual(
            tokenize_name('uint8ArrayMember'), ['uint8', 'Array', 'Member'])
        self.assertEqual(tokenize_name('webgl2Element'), ['webgl2', 'Element'])
        self.assertEqual(tokenize_name('webGL2Element'), ['webGL2', 'Element'])
        self.assertEqual(tokenize_name('xssError'), ['xss', 'Error'])

        self.assertEqual(tokenize_name('FileURLs'), ['File', 'URLs'])

        self.assertEqual(
            tokenize_name('XRDOMOverlay'), ['XR', 'DOM', 'Overlay'])

    def test_ignoring_characters(self):
        self.assertEqual(tokenize_name('Animation.idl'), ['Animation', 'idl'])
        self.assertEqual(
            tokenize_name('-webkit-appearance'), ['webkit', 'appearance'])
        self.assertEqual(tokenize_name('  foo_bar!#"$'), ['foo', 'bar'])


class NameStyleConverterTest(unittest.TestCase):
    def test_original(self):
        self.assertEqual(
            NameStyleConverter('-webkit-appearance').original,
            '-webkit-appearance')

    def test_snake_case(self):
        converter = NameStyleConverter('HTMLElement')
        self.assertEqual(converter.to_snake_case(), 'html_element')
        self.assertEqual(
            NameStyleConverter('FileURLs').to_snake_case(), 'file_urls')

    def test_to_class_data_member(self):
        converter = NameStyleConverter('HTMLElement')
        self.assertEqual(converter.to_class_data_member(), 'html_element_')
        self.assertEqual(
            converter.to_class_data_member(prefix='is'), 'is_html_element_')
        self.assertEqual(
            converter.to_class_data_member(suffix='enabled'),
            'html_element_enabled_')
        self.assertEqual(
            converter.to_class_data_member(prefix='is', suffix='enabled'),
            'is_html_element_enabled_')
        self.assertEqual(
            converter.to_class_data_member(prefix='fooBar', suffix='V0V8'),
            'foobar_html_element_v0v8_')

    def test_upper_camel_case(self):
        converter = NameStyleConverter('someSuperThing')
        self.assertEqual(converter.to_upper_camel_case(), 'SomeSuperThing')
        converter = NameStyleConverter('SVGElement')
        self.assertEqual(converter.to_upper_camel_case(), 'SVGElement')
        converter = NameStyleConverter('cssExternalScannerPreload')
        self.assertEqual(converter.to_upper_camel_case(),
                         'CSSExternalScannerPreload')
        converter = NameStyleConverter('xpathExpression')
        self.assertEqual(converter.to_upper_camel_case(), 'XPathExpression')
        converter = NameStyleConverter('feDropShadow')
        self.assertEqual(converter.to_upper_camel_case(), 'FEDropShadow')

    def test_to_class_name(self):
        self.assertEqual(NameStyleConverter('').to_class_name(), '')
        self.assertEqual(
            NameStyleConverter('').to_class_name(prefix='s', suffix='d'), 'SD')
        self.assertEqual(
            NameStyleConverter('').to_class_name(
                prefix='style', suffix='data'), 'StyleData')
        self.assertEqual(
            NameStyleConverter('foo').to_class_name(
                prefix='style', suffix='data'), 'StyleFooData')
        self.assertEqual(NameStyleConverter('xpath').to_class_name(), 'XPath')

    def test_to_function_name(self):
        converter = NameStyleConverter('fooBar')
        self.assertEqual(converter.to_function_name(), 'FooBar')
        self.assertEqual(converter.to_function_name(prefix='is'), 'IsFooBar')
        self.assertEqual(converter.to_function_name(suffix='BAZ'), 'FooBarBaz')
        self.assertEqual(
            converter.to_function_name(prefix='IS', suffix='baz'),
            'IsFooBarBaz')
        self.assertEqual(
            converter.to_function_name(
                prefix='prefixPrefix', suffix=['a', 'b']),
            'PrefixprefixFooBarAB')

    def test_to_enum_value(self):
        self.assertEqual(
            NameStyleConverter('fooBar').to_enum_value(), 'kFooBar')

    def test_lower_camel_case(self):
        converter = NameStyleConverter('someSuperThing')
        self.assertEqual(converter.to_lower_camel_case(), 'someSuperThing')
        converter = NameStyleConverter('SVGElement')
        self.assertEqual(converter.to_lower_camel_case(), 'svgElement')
        converter = NameStyleConverter('documentURI')
        self.assertEqual(converter.to_lower_camel_case(), 'documentURI')
        converter = NameStyleConverter('-webkit-margin-start')
        self.assertEqual(converter.to_lower_camel_case(), 'webkitMarginStart')
        converter = NameStyleConverter('Accelerated2dCanvas')
        self.assertEqual(converter.to_lower_camel_case(),
                         'accelerated2dCanvas')

    def test_macro_case(self):
        converter = NameStyleConverter('WebGLBaz2D')
        self.assertEqual(converter.to_macro_case(), 'WEBGL_BAZ_2D')

    def test_all_cases(self):
        converter = NameStyleConverter('SVGScriptElement')
        self.assertEqual(
            converter.to_all_cases(), {
                'snake_case': 'svg_script_element',
                'upper_camel_case': 'SVGScriptElement',
                'macro_case': 'SVG_SCRIPT_ELEMENT',
            })

    def test_to_header_guard(self):
        converter = NameStyleConverter(
            'third_party/blink/renderer/bindings/modules/v8/v8_path_2d.h')
        self.assertEqual(
            converter.to_header_guard(),
            'THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_V8_PATH_2D_H_')

    def test_equality(self):
        a_1 = NameStyleConverter('a')
        a_2 = NameStyleConverter('a')
        c = NameStyleConverter('c')

        self.assertEqual(a_1, a_2)
        self.assertNotEqual(a_1, c)
        self.assertEqual(hash(a_1), hash(a_2))
        self.assertNotEqual(hash(a_1), hash(c))


if __name__ == '__main__':
    unittest.main()
