# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from mojom.parse import ast
from mojom.parse import lexer
from mojom.parse import parser

class ParserTest(unittest.TestCase):
  """Tests |parser.Parse()|."""

  def testTrivialValidSource(self):
    """Tests a trivial, but valid, .mojom source."""

    source = """\
        // This is a comment.

        module my_module;
        """
    expected = ast.Mojom(ast.Module(ast.Identifier('my_module'), None),
                         ast.ImportList(), [])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testSourceWithCrLfs(self):
    """Tests a .mojom source with CR-LFs instead of LFs."""

    source = "// This is a comment.\r\n\r\nmodule my_module;\r\n"
    expected = ast.Mojom(ast.Module(ast.Identifier('my_module'), None),
                         ast.ImportList(), [])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testUnexpectedEOF(self):
    """Tests a "truncated" .mojom source."""

    source = """\
        // This is a comment.

        module my_module
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom: Error: Unexpected end of file$"):
      parser.Parse(source, "my_file.mojom")

  def testCommentLineNumbers(self):
    """Tests that line numbers are correctly tracked when comments are
    present."""

    source1 = """\
        // Isolated C++-style comments.

        // Foo.
        asdf1
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:4: Error: Unexpected 'asdf1':\n *asdf1$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        // Consecutive C++-style comments.
        // Foo.
        // Bar.

        struct Yada {  // Baz.
                       // Quux.
          int32 x;
        };

        asdf2
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:10: Error: Unexpected 'asdf2':\n *asdf2$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        /* Single-line C-style comments. */
        /* Foobar. */

        /* Baz. */
        asdf3
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:5: Error: Unexpected 'asdf3':\n *asdf3$"):
      parser.Parse(source3, "my_file.mojom")

    source4 = """\
        /* Multi-line C-style comments.
        */
        /*
        Foo.
        Bar.
        */

        /* Baz
           Quux. */
        asdf4
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:10: Error: Unexpected 'asdf4':\n *asdf4$"):
      parser.Parse(source4, "my_file.mojom")

  def testSimpleStruct(self):
    """Tests a simple .mojom source that just defines a struct."""

    source = """\
        module my_module;

        struct MyStruct {
          int32 a;
          double b;
        };
        """
    expected = ast.Mojom(
        ast.Module(ast.Identifier('my_module'),
                   None), ast.ImportList(), [
                       ast.Struct(
                           ast.Name('MyStruct'), None,
                           ast.StructBody([
                               ast.StructField(
                                   ast.Name('a'), None, None,
                                   ast.Typename(ast.Identifier('int32')), None),
                               ast.StructField(
                                   ast.Name('b'), None, None,
                                   ast.Typename(ast.Identifier('double')), None)
                           ]))
                   ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testSimpleStructWithoutModule(self):
    """Tests a simple struct without an explict module statement."""

    source = """\
        struct MyStruct {
          int32 a;
          double b;
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(ast.Name('a'), None, None,
                                ast.Typename(ast.Identifier('int32')), None),
                ast.StructField(ast.Name('b'), None, None,
                                ast.Typename(ast.Identifier('double')), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidStructDefinitions(self):
    """Tests all types of definitions that can occur in a struct."""

    source = """\
        struct MyStruct {
          enum MyEnum { VALUE };
          const double kMyConst = 1.23;
          int32 a;
          SomeOtherStruct b;  // Invalidity detected at another stage.
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.Enum(
                    ast.Name('MyEnum'), None,
                    ast.EnumValueList(
                        ast.EnumValue(ast.Name('VALUE'), None, None))),
                ast.Const(ast.Name('kMyConst'), None,
                          ast.Typename(ast.Identifier('double')),
                          ast.Literal('float', '1.23')),
                ast.StructField(ast.Name('a'), None, None,
                                ast.Typename(ast.Identifier('int32')), None),
                ast.StructField(ast.Name('b'), None, None,
                                ast.Typename(ast.Identifier('SomeOtherStruct')),
                                None)
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidStructDefinitions(self):
    """Tests that definitions that aren't allowed in a struct are correctly
    detected."""

    source1 = """\
        struct MyStruct {
          MyMethod(int32 a);
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected '\(':\n"
        r" *MyMethod\(int32 a\);$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          struct MyInnerStruct {
            int32 a;
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'struct':\n"
        r" *struct MyInnerStruct {$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        struct MyStruct {
          interface MyInterface {
            MyMethod(int32 a);
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'interface':\n"
        r" *interface MyInterface {$"):
      parser.Parse(source3, "my_file.mojom")

  def testMissingModuleName(self):
    """Tests an (invalid) .mojom with a missing module name."""

    source1 = """\
        // Missing module name.
        module ;
        struct MyStruct {
          int32 a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected ';':\n *module ;$"):
      parser.Parse(source1, "my_file.mojom")

    # Another similar case, but make sure that line-number tracking/reporting
    # is correct.
    source2 = """\
        module
        // This line intentionally left unblank.

        struct MyStruct {
          int32 a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:4: Error: Unexpected 'struct':\n"
        r" *struct MyStruct {$"):
      parser.Parse(source2, "my_file.mojom")

  def testMultipleModuleStatements(self):
    """Tests an (invalid) .mojom with multiple module statements."""

    source = """\
        module foo;
        module bar;
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Multiple \"module\" statements not "
        r"allowed:\n *module bar;$"):
      parser.Parse(source, "my_file.mojom")

  def testModuleStatementAfterImport(self):
    """Tests an (invalid) .mojom with a module statement after an import."""

    source = """\
        import "foo.mojom";
        module foo;
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: \"module\" statements must precede imports "
        r"and definitions:\n *module foo;$"):
      parser.Parse(source, "my_file.mojom")

  def testModuleStatementAfterDefinition(self):
    """Tests an (invalid) .mojom with a module statement after a definition."""

    source = """\
        struct MyStruct {
          int32 a;
        };
        module foo;
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:4: Error: \"module\" statements must precede imports "
        r"and definitions:\n *module foo;$"):
      parser.Parse(source, "my_file.mojom")

  def testImportStatementAfterDefinition(self):
    """Tests an (invalid) .mojom with an import statement after a definition."""

    source = """\
        struct MyStruct {
          int32 a;
        };
        import "foo.mojom";
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:4: Error: \"import\" statements must precede "
        r"definitions:\n *import \"foo.mojom\";$"):
      parser.Parse(source, "my_file.mojom")

  def testEnums(self):
    """Tests that enum statements are correctly parsed."""

    source = """\
        module my_module;
        enum MyEnum1 { VALUE1, VALUE2 };  // No trailing comma.
        enum MyEnum2 {
          VALUE1 = -1,
          VALUE2 = 0,
          VALUE3 = + 987,  // Check that space is allowed.
          VALUE4 = 0xAF12,
          VALUE5 = -0x09bcd,
          VALUE6 = VALUE5,
          VALUE7,  // Leave trailing comma.
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Enum(
                ast.Name('MyEnum1'), None,
                ast.EnumValueList([
                    ast.EnumValue(ast.Name('VALUE1'), None, None),
                    ast.EnumValue(ast.Name('VALUE2'), None, None)
                ])),
            ast.Enum(
                ast.Name('MyEnum2'), None,
                ast.EnumValueList([
                    ast.EnumValue(ast.Name('VALUE1'), None,
                                  ast.Literal('int', '-1')),
                    ast.EnumValue(ast.Name('VALUE2'), None,
                                  ast.Literal('int', '0')),
                    ast.EnumValue(ast.Name('VALUE3'), None,
                                  ast.Literal('int', '+987')),
                    ast.EnumValue(ast.Name('VALUE4'), None,
                                  ast.Literal('int', '0xAF12')),
                    ast.EnumValue(ast.Name('VALUE5'), None,
                                  ast.Literal('int', '-0x09bcd')),
                    ast.EnumValue(ast.Name('VALUE6'), None,
                                  ast.Identifier('VALUE5')),
                    ast.EnumValue(ast.Name('VALUE7'), None, None)
                ]))
        ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidEnumInitializers(self):
    """Tests that invalid enum initializers are correctly detected."""

    # Floating point value.
    source2 = "enum MyEnum { VALUE = 0.123 };"
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:1: Error: Unexpected '0\.123':\n"
        r"enum MyEnum { VALUE = 0\.123 };$"):
      parser.Parse(source2, "my_file.mojom")

    # Boolean value.
    source2 = "enum MyEnum { VALUE = true };"
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:1: Error: Unexpected 'true':\n"
        r"enum MyEnum { VALUE = true };$"):
      parser.Parse(source2, "my_file.mojom")

  def testConsts(self):
    """Tests some constants and struct members initialized with them."""

    source = """\
        module my_module;

        struct MyStruct {
          const int8 kNumber = -1;
          int8 number@0 = kNumber;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Struct(
                ast.Name('MyStruct'), None,
                ast.StructBody([
                    ast.Const(ast.Name('kNumber'), None,
                              ast.Typename(ast.Identifier('int8')),
                              ast.Literal('int', '-1')),
                    ast.StructField(ast.Name('number'), None, ast.Ordinal(0),
                                    ast.Typename(ast.Identifier('int8')),
                                    ast.Identifier('kNumber'))
                ]))
        ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testNoConditionals(self):
    """Tests that ?: is not allowed."""

    source = """\
        module my_module;

        enum MyEnum {
          MY_ENUM_1 = 1 ? 2 : 3
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:4: Error: Unexpected '\?':\n"
        r" *MY_ENUM_1 = 1 \? 2 : 3$"):
      parser.Parse(source, "my_file.mojom")

  def testSimpleOrdinals(self):
    """Tests that (valid) ordinal values are scanned correctly."""

    source = """\
        module my_module;

        // This isn't actually valid .mojom, but the problem (missing ordinals)
        // should be handled at a different level.
        struct MyStruct {
          int32 a0@0;
          int32 a1@1;
          int32 a2@2;
          int32 a9@9;
          int32 a10 @10;
          int32 a11 @11;
          int32 a29 @29;
          int32 a1234567890 @1234567890;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Struct(
                ast.Name('MyStruct'), None,
                ast.StructBody([
                    ast.StructField(ast.Name('a0'), None, ast.Ordinal(0),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a1'), None, ast.Ordinal(1),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a2'), None, ast.Ordinal(2),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a9'), None, ast.Ordinal(9),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a10'), None, ast.Ordinal(10),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a11'), None, ast.Ordinal(11),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a29'), None, ast.Ordinal(29),
                                    ast.Typename(ast.Identifier('int32')),
                                    None),
                    ast.StructField(ast.Name('a1234567890'), None,
                                    ast.Ordinal(1234567890),
                                    ast.Typename(ast.Identifier('int32')), None)
                ]))
        ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidOrdinals(self):
    """Tests that (lexically) invalid ordinals are correctly detected."""

    source1 = """\
        module my_module;

        struct MyStruct {
          int32 a_missing@;
        };
        """
    with self.assertRaisesRegexp(
        lexer.LexError, r"^my_file\.mojom:4: Error: Missing ordinal value$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        module my_module;

        struct MyStruct {
          int32 a_octal@01;
        };
        """
    with self.assertRaisesRegexp(
        lexer.LexError, r"^my_file\.mojom:4: Error: "
        r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        module my_module; struct MyStruct { int32 a_invalid_octal@08; };
        """
    with self.assertRaisesRegexp(
        lexer.LexError, r"^my_file\.mojom:1: Error: "
        r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source3, "my_file.mojom")

    source4 = "module my_module; struct MyStruct { int32 a_hex@0x1aB9; };"
    with self.assertRaisesRegexp(
        lexer.LexError, r"^my_file\.mojom:1: Error: "
        r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source4, "my_file.mojom")

    source5 = "module my_module; struct MyStruct { int32 a_hex@0X0; };"
    with self.assertRaisesRegexp(
        lexer.LexError, r"^my_file\.mojom:1: Error: "
        r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source5, "my_file.mojom")

    source6 = """\
        struct MyStruct {
          int32 a_too_big@999999999999;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: "
        r"Ordinal value 999999999999 too large:\n"
        r" *int32 a_too_big@999999999999;$"):
      parser.Parse(source6, "my_file.mojom")

  def testNestedNamespace(self):
    """Tests that "nested" namespaces work."""

    source = """\
        module my.mod;

        struct MyStruct {
          int32 a;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my.mod'), None), ast.ImportList(), [
            ast.Struct(
                ast.Name('MyStruct'), None,
                ast.StructBody(
                    ast.StructField(ast.Name('a'), None, None,
                                    ast.Typename(ast.Identifier('int32')),
                                    None)))
        ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidHandleTypes(self):
    """Tests (valid) handle types."""

    source = """\
        struct MyStruct {
          handle a;
          handle<data_pipe_consumer> b;
          handle <data_pipe_producer> c;
          handle < message_pipe > d;
          handle
            < shared_buffer
            > e;
          handle
            <platform

            > f;
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(ast.Name('a'), None, None,
                                ast.Typename(ast.Identifier('handle')), None),
                ast.StructField(
                    ast.Name('b'), None, None,
                    ast.Typename(ast.Identifier('handle<data_pipe_consumer>')),
                    None),
                ast.StructField(
                    ast.Name('c'), None, None,
                    ast.Typename(ast.Identifier('handle<data_pipe_producer>')),
                    None),
                ast.StructField(
                    ast.Name('d'), None, None,
                    ast.Typename(ast.Identifier('handle<message_pipe>')), None),
                ast.StructField(
                    ast.Name('e'), None, None,
                    ast.Typename(ast.Identifier('handle<shared_buffer>')),
                    None),
                ast.StructField(
                    ast.Name('f'), None, None,
                    ast.Typename(ast.Identifier('handle<platform>')), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidHandleType(self):
    """Tests an invalid (unknown) handle type."""

    source = """\
        struct MyStruct {
          handle<wtf_is_this> foo;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: "
        r"Invalid handle type 'wtf_is_this':\n"
        r" *handle<wtf_is_this> foo;$"):
      parser.Parse(source, "my_file.mojom")

  def testValidDefaultValues(self):
    """Tests default values that are valid (to the parser)."""

    source = """\
        struct MyStruct {
          int16 a0 = 0;
          uint16 a1 = 0x0;
          uint16 a2 = 0x00;
          uint16 a3 = 0x01;
          uint16 a4 = 0xcd;
          int32 a5 = 12345;
          int64 a6 = -12345;
          int64 a7 = +12345;
          uint32 a8 = 0x12cd3;
          uint32 a9 = -0x12cD3;
          uint32 a10 = +0x12CD3;
          bool a11 = true;
          bool a12 = false;
          float a13 = 1.2345;
          float a14 = -1.2345;
          float a15 = +1.2345;
          float a16 = 123.;
          float a17 = .123;
          double a18 = 1.23E10;
          double a19 = 1.E-10;
          double a20 = .5E+10;
          double a21 = -1.23E10;
          double a22 = +.123E10;
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(ast.Name('a0'), None, None,
                                ast.Typename(ast.Identifier('int16')),
                                ast.Literal('int', '0')),
                ast.StructField(ast.Name('a1'), None, None,
                                ast.Typename(ast.Identifier('uint16')),
                                ast.Literal('int', '0x0')),
                ast.StructField(ast.Name('a2'), None, None,
                                ast.Typename(ast.Identifier('uint16')),
                                ast.Literal('int', '0x00')),
                ast.StructField(ast.Name('a3'), None, None,
                                ast.Typename(ast.Identifier('uint16')),
                                ast.Literal('int', '0x01')),
                ast.StructField(ast.Name('a4'), None, None,
                                ast.Typename(ast.Identifier('uint16')),
                                ast.Literal('int', '0xcd')),
                ast.StructField(ast.Name('a5'), None, None,
                                ast.Typename(ast.Identifier('int32')),
                                ast.Literal('int', '12345')),
                ast.StructField(ast.Name('a6'), None, None,
                                ast.Typename(ast.Identifier('int64')),
                                ast.Literal('int', '-12345')),
                ast.StructField(ast.Name('a7'), None, None,
                                ast.Typename(ast.Identifier('int64')),
                                ast.Literal('int', '+12345')),
                ast.StructField(ast.Name('a8'), None, None,
                                ast.Typename(ast.Identifier('uint32')),
                                ast.Literal('int', '0x12cd3')),
                ast.StructField(ast.Name('a9'), None, None,
                                ast.Typename(ast.Identifier('uint32')),
                                ast.Literal('int', '-0x12cD3')),
                ast.StructField(ast.Name('a10'), None, None,
                                ast.Typename(ast.Identifier('uint32')),
                                ast.Literal('int', '+0x12CD3')),
                ast.StructField(ast.Name('a11'), None, None,
                                ast.Typename(ast.Identifier('bool')),
                                ast.Literal('TRUE', 'true')),
                ast.StructField(ast.Name('a12'), None, None,
                                ast.Typename(ast.Identifier('bool')),
                                ast.Literal('FALSE', 'false')),
                ast.StructField(ast.Name('a13'), None, None,
                                ast.Typename(ast.Identifier('float')),
                                ast.Literal('float', '1.2345')),
                ast.StructField(ast.Name('a14'), None, None,
                                ast.Typename(ast.Identifier('float')),
                                ast.Literal('float', '-1.2345')),
                ast.StructField(ast.Name('a15'), None, None,
                                ast.Typename(ast.Identifier('float')),
                                ast.Literal('float', '+1.2345')),
                ast.StructField(ast.Name('a16'), None, None,
                                ast.Typename(ast.Identifier('float')),
                                ast.Literal('float', '123.')),
                ast.StructField(ast.Name('a17'), None, None,
                                ast.Typename(ast.Identifier('float')),
                                ast.Literal('float', '.123')),
                ast.StructField(ast.Name('a18'), None, None,
                                ast.Typename(ast.Identifier('double')),
                                ast.Literal('float', '1.23E10')),
                ast.StructField(ast.Name('a19'), None, None,
                                ast.Typename(ast.Identifier('double')),
                                ast.Literal('float', '1.E-10')),
                ast.StructField(ast.Name('a20'), None, None,
                                ast.Typename(ast.Identifier('double')),
                                ast.Literal('float', '.5E+10')),
                ast.StructField(ast.Name('a21'), None, None,
                                ast.Typename(ast.Identifier('double')),
                                ast.Literal('float', '-1.23E10')),
                ast.StructField(ast.Name('a22'), None, None,
                                ast.Typename(ast.Identifier('double')),
                                ast.Literal('float', '+.123E10'))
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidFixedSizeArray(self):
    """Tests parsing a fixed size array."""

    source = """\
        struct MyStruct {
          array<int32> normal_array;
          array<int32, 1> fixed_size_array_one_entry;
          array<int32, 10> fixed_size_array_ten_entries;
          array<array<array<int32, 1>>, 2> nested_arrays;
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(
                    ast.Name('normal_array'), None, None,
                    ast.Typename(
                        ast.Array(ast.Typename(ast.Identifier('int32')))),
                    None),
                ast.StructField(
                    ast.Name('fixed_size_array_one_entry'), None, None,
                    ast.Typename(
                        ast.Array(ast.Typename(ast.Identifier('int32')),
                                  fixed_size=1)), None),
                ast.StructField(
                    ast.Name('fixed_size_array_ten_entries'), None, None,
                    ast.Typename(
                        ast.Array(ast.Typename(ast.Identifier('int32')),
                                  fixed_size=10)), None),
                ast.StructField(
                    ast.Name('nested_arrays'), None, None,
                    ast.Typename(
                        ast.Array(ast.Typename(
                            ast.Array(
                                ast.Typename(
                                    ast.Array(ast.Typename(
                                        ast.Identifier('int32')),
                                              fixed_size=1)))),
                                  fixed_size=2)), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidNestedArray(self):
    """Tests parsing a nested array."""

    source = "struct MyStruct { array<array<int32>> nested_array; };"
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody(
                ast.StructField(
                    ast.Name('nested_array'), None, None,
                    ast.Typename(
                        ast.Array(
                            ast.Typename(
                                ast.Array(ast.Typename(
                                    ast.Identifier('int32')))))), None)))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidFixedArraySize(self):
    """Tests that invalid fixed array bounds are correctly detected."""

    source1 = """\
        struct MyStruct {
          array<int32, 0> zero_size_array;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Fixed array size 0 invalid:\n"
        r" *array<int32, 0> zero_size_array;$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          array<int32, 999999999999> too_big_array;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Fixed array size 999999999999 invalid:\n"
        r" *array<int32, 999999999999> too_big_array;$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        struct MyStruct {
          array<int32, abcdefg> not_a_number;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'abcdefg':\n"
        r" *array<int32, abcdefg> not_a_number;"):
      parser.Parse(source3, "my_file.mojom")

  def testValidAssociativeArrays(self):
    """Tests that we can parse valid associative array structures."""

    source1 = "struct MyStruct { map<string, uint8> data; };"
    expected1 = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(
                    ast.Name('data'), None, None,
                    ast.Typename(
                        ast.Map(ast.Identifier('string'),
                                ast.Typename(ast.Identifier('uint8')))), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    source2 = "interface MyInterface { MyMethod(map<string, uint8> a); };"
    expected2 = ast.Mojom(None, ast.ImportList(), [
        ast.Interface(
            ast.Name('MyInterface'), None,
            ast.InterfaceBody(
                ast.Method(
                    ast.Name('MyMethod'), None, None,
                    ast.ParameterList(
                        ast.Parameter(
                            ast.Name('a'), None, None,
                            ast.Typename(
                                ast.Map(ast.Identifier('string'),
                                        ast.Typename(
                                            ast.Identifier('uint8')))))),
                    None)))
    ])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    source3 = "struct MyStruct { map<string, array<uint8>> data; };"
    expected3 = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(
                    ast.Name('data'), None, None,
                    ast.Typename(
                        ast.Map(
                            ast.Identifier('string'),
                            ast.Typename(
                                ast.Array(ast.Typename(
                                    ast.Identifier('uint8')))))), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

  def testValidMethod(self):
    """Tests parsing method declarations."""

    source1 = "interface MyInterface { MyMethod(int32 a); };"
    expected1 = ast.Mojom(None, ast.ImportList(), [
        ast.Interface(
            ast.Name('MyInterface'), None,
            ast.InterfaceBody(
                ast.Method(
                    ast.Name('MyMethod'), None, None,
                    ast.ParameterList(
                        ast.Parameter(ast.Name('a'), None, None,
                                      ast.Typename(ast.Identifier('int32')))),
                    None)))
    ])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    source2 = """\
        interface MyInterface {
          MyMethod1@0(int32 a@0, int64 b@1);
          MyMethod2@1() => ();
        };
        """
    expected2 = ast.Mojom(None, ast.ImportList(), [
        ast.Interface(
            ast.Name('MyInterface'), None,
            ast.InterfaceBody([
                ast.Method(
                    ast.Name('MyMethod1'), None, ast.Ordinal(0),
                    ast.ParameterList([
                        ast.Parameter(ast.Name('a'), None, ast.Ordinal(0),
                                      ast.Typename(ast.Identifier('int32'))),
                        ast.Parameter(ast.Name('b'), None, ast.Ordinal(1),
                                      ast.Typename(ast.Identifier('int64')))
                    ]), None),
                ast.Method(ast.Name('MyMethod2'), None, ast.Ordinal(1),
                           ast.ParameterList(), ast.ParameterList())
            ]))
    ])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    source3 = """\
        interface MyInterface {
          MyMethod(string a) => (int32 a, bool b);
        };
        """
    expected3 = ast.Mojom(None, ast.ImportList(), [
        ast.Interface(
            ast.Name('MyInterface'), None,
            ast.InterfaceBody(
                ast.Method(
                    ast.Name('MyMethod'), None, None,
                    ast.ParameterList(
                        ast.Parameter(ast.Name('a'), None, None,
                                      ast.Typename(ast.Identifier('string')))),
                    ast.ParameterList([
                        ast.Parameter(ast.Name('a'), None, None,
                                      ast.Typename(ast.Identifier('int32'))),
                        ast.Parameter(ast.Name('b'), None, None,
                                      ast.Typename(ast.Identifier('bool')))
                    ]))))
    ])
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

  def testInvalidMethods(self):
    """Tests that invalid method declarations are correctly detected."""

    # No trailing commas.
    source1 = """\
        interface MyInterface {
          MyMethod(string a,);
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected '\)':\n"
        r" *MyMethod\(string a,\);$"):
      parser.Parse(source1, "my_file.mojom")

    # No leading commas.
    source2 = """\
        interface MyInterface {
          MyMethod(, string a);
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected ',':\n"
        r" *MyMethod\(, string a\);$"):
      parser.Parse(source2, "my_file.mojom")

  def testValidInterfaceDefinitions(self):
    """Tests all types of definitions that can occur in an interface."""

    source = """\
        interface MyInterface {
          enum MyEnum { VALUE };
          const int32 kMyConst = 123;
          MyMethod(int32 x) => (MyEnum y);
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Interface(
            ast.Name('MyInterface'), None,
            ast.InterfaceBody([
                ast.Enum(
                    ast.Name('MyEnum'), None,
                    ast.EnumValueList(
                        ast.EnumValue(ast.Name('VALUE'), None, None))),
                ast.Const(ast.Name('kMyConst'), None,
                          ast.Typename(ast.Identifier('int32')),
                          ast.Literal('int', '123')),
                ast.Method(
                    ast.Name('MyMethod'), None, None,
                    ast.ParameterList(
                        ast.Parameter(ast.Name('x'), None, None,
                                      ast.Typename(ast.Identifier('int32')))),
                    ast.ParameterList(
                        ast.Parameter(ast.Name('y'), None, None,
                                      ast.Typename(ast.Identifier('MyEnum')))))
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidInterfaceDefinitions(self):
    """Tests that definitions that aren't allowed in an interface are correctly
    detected."""

    source1 = """\
        interface MyInterface {
          struct MyStruct {
            int32 a;
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'struct':\n"
        r" *struct MyStruct {$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        interface MyInterface {
          interface MyInnerInterface {
            MyMethod(int32 x);
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'interface':\n"
        r" *interface MyInnerInterface {$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        interface MyInterface {
          int32 my_field;
        };
        """
    # The parser thinks that "int32" is a plausible name for a method, so it's
    # "my_field" that gives it away.
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'my_field':\n"
        r" *int32 my_field;$"):
      parser.Parse(source3, "my_file.mojom")

  def testValidAttributes(self):
    """Tests parsing attributes (and attribute lists)."""

    # Note: We use structs because they have (optional) attribute lists.

    # Empty attribute list.
    source1 = "[] struct MyStruct {};"
    expected1 = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(ast.Name('MyStruct'), ast.AttributeList(), ast.StructBody())
    ])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    # One-element attribute list, with name value.
    source2 = "[MyAttribute=MyName] struct MyStruct {};"
    expected2 = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'),
            ast.AttributeList(
                ast.Attribute(ast.Name("MyAttribute"), ast.Name("MyName"))),
            ast.StructBody())
    ])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    # Two-element attribute list, with one string value and one integer value.
    source3 = "[MyAttribute1 = \"hello\", MyAttribute2 = 5] struct MyStruct {};"
    expected3 = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'),
            ast.AttributeList([
                ast.Attribute(ast.Name("MyAttribute1"), "hello"),
                ast.Attribute(ast.Name("MyAttribute2"), 5)
            ]), ast.StructBody())
    ])
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

    # Various places that attribute list is allowed.
    source4 = """\
        [Attr0=0] module my_module;

        [Attr1=1] import "my_import";

        [Attr2=2] struct MyStruct {
          [Attr3=3] int32 a;
        };
        [Attr4=4] union MyUnion {
          [Attr5=5] int32 a;
        };
        [Attr6=6] enum MyEnum {
          [Attr7=7] a
        };
        [Attr8=8] interface MyInterface {
          [Attr9=9] MyMethod([Attr10=10] int32 a) => ([Attr11=11] bool b);
        };
        [Attr12=12] const double kMyConst = 1.23;
        """
    expected4 = ast.Mojom(
        ast.Module(ast.Identifier('my_module'),
                   ast.AttributeList([ast.Attribute(ast.Name("Attr0"), 0)])),
        ast.ImportList(
            ast.Import(ast.AttributeList([ast.Attribute(ast.Name("Attr1"), 1)]),
                       "my_import")),
        [
            ast.Struct(
                ast.Name('MyStruct'),
                ast.AttributeList(ast.Attribute(ast.Name("Attr2"), 2)),
                ast.StructBody(
                    ast.StructField(
                        ast.Name('a'),
                        ast.AttributeList([ast.Attribute(ast.Name("Attr3"), 3)
                                           ]), None,
                        ast.Typename(ast.Identifier('int32')), None))),
            ast.Union(
                ast.Name('MyUnion'),
                ast.AttributeList(ast.Attribute(ast.Name("Attr4"), 4)),
                ast.UnionBody(
                    ast.UnionField(
                        ast.Name('a'),
                        ast.AttributeList([ast.Attribute(ast.Name("Attr5"), 5)
                                           ]), None,
                        ast.Typename(ast.Identifier('int32'))))),
            ast.Enum(
                ast.Name('MyEnum'),
                ast.AttributeList(ast.Attribute(ast.Name("Attr6"), 6)),
                ast.EnumValueList(
                    ast.EnumValue(
                        ast.Name('VALUE'),
                        ast.AttributeList([ast.Attribute(ast.Name("Attr7"), 7)
                                           ]), None))),
            ast.Interface(
                ast.Name('MyInterface'),
                ast.AttributeList(ast.Attribute(ast.Name("Attr8"), 8)),
                ast.InterfaceBody(
                    ast.Method(
                        ast.Name('MyMethod'),
                        ast.AttributeList(ast.Attribute(ast.Name("Attr9"), 9)),
                        None,
                        ast.ParameterList(
                            ast.Parameter(
                                ast.Name('a'),
                                ast.AttributeList(
                                    [ast.Attribute(ast.Name("Attr10"), 10)]),
                                None, ast.Typename(ast.Identifier('int32')))),
                        ast.ParameterList(
                            ast.Parameter(
                                ast.Name('b'),
                                ast.AttributeList(
                                    [ast.Attribute(ast.Name("Attr11"), 11)]),
                                None, ast.Typename(ast.Identifier('bool'))))))),
            ast.Const(ast.Name('kMyConst'),
                      ast.AttributeList(ast.Attribute(ast.Name("Attr12"), 12)),
                      ast.Typename(ast.Identifier('double')),
                      ast.Literal('float', '1.23'))
        ])
    self.assertEquals(parser.Parse(source4, "my_file.mojom"), expected4)

    # TODO(vtl): Boolean attributes don't work yet. (In fact, we just |eval()|
    # literal (non-name) values, which is extremely dubious.)

  def testInvalidAttributes(self):
    """Tests that invalid attributes and attribute lists are correctly
    detected."""

    # Trailing commas not allowed.
    source1 = "[MyAttribute=MyName,] struct MyStruct {};"
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:1: Error: Unexpected '\]':\n"
        r"\[MyAttribute=MyName,\] struct MyStruct {};$"):
      parser.Parse(source1, "my_file.mojom")

    # Missing value.
    source2 = "[MyAttribute=] struct MyStruct {};"
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:1: Error: Unexpected '\]':\n"
        r"\[MyAttribute=\] struct MyStruct {};$"):
      parser.Parse(source2, "my_file.mojom")

    # Missing key.
    source3 = "[=MyName] struct MyStruct {};"
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:1: Error: Unexpected '=':\n"
        r"\[=MyName\] struct MyStruct {};$"):
      parser.Parse(source3, "my_file.mojom")

  def testValidImports(self):
    """Tests parsing import statements."""

    # One import (no module statement).
    source1 = "import \"somedir/my.mojom\";"
    expected1 = ast.Mojom(None,
                          ast.ImportList(ast.Import(None, "somedir/my.mojom")),
                          [])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    # Two imports (no module statement).
    source2 = """\
        import "somedir/my1.mojom";
        import "somedir/my2.mojom";
        """
    expected2 = ast.Mojom(
        None,
        ast.ImportList([
            ast.Import(None, "somedir/my1.mojom"),
            ast.Import(None, "somedir/my2.mojom")
        ]), [])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    # Imports with module statement.
    source3 = """\
        module my_module;
        import "somedir/my1.mojom";
        import "somedir/my2.mojom";
        """
    expected3 = ast.Mojom(
        ast.Module(ast.Identifier('my_module'), None),
        ast.ImportList([
            ast.Import(None, "somedir/my1.mojom"),
            ast.Import(None, "somedir/my2.mojom")
        ]), [])
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

  def testInvalidImports(self):
    """Tests that invalid import statements are correctly detected."""

    source1 = """\
        // Make the error occur on line 2.
        import invalid
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'invalid':\n"
        r" *import invalid$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        import  // Missing string.
        struct MyStruct {
          int32 a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'struct':\n"
        r" *struct MyStruct {$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        import "foo.mojom"  // Missing semicolon.
        struct MyStruct {
          int32 a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected 'struct':\n"
        r" *struct MyStruct {$"):
      parser.Parse(source3, "my_file.mojom")

  def testValidNullableTypes(self):
    """Tests parsing nullable types."""

    source = """\
        struct MyStruct {
          int32? a;  // This is actually invalid, but handled at a different
                     // level.
          string? b;
          array<int32> ? c;
          array<string ? > ? d;
          array<array<int32>?>? e;
          array<int32, 1>? f;
          array<string?, 1>? g;
          some_struct? h;
          handle? i;
          handle<data_pipe_consumer>? j;
          handle<data_pipe_producer>? k;
          handle<message_pipe>? l;
          handle<shared_buffer>? m;
          pending_receiver<some_interface>? n;
          handle<platform>? o;
        };
        """
    expected = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(
                    ast.Name('a'), None, None,
                    ast.Typename(ast.Identifier('int32'), nullable=True), None),
                ast.StructField(
                    ast.Name('b'), None, None,
                    ast.Typename(ast.Identifier('string'), nullable=True),
                    None),
                ast.StructField(
                    ast.Name('c'), None, None,
                    ast.Typename(ast.Array(ast.Typename(
                        ast.Identifier('int32'))),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('d'), None, None,
                    ast.Typename(ast.Array(
                        ast.Typename(ast.Identifier('string'), nullable=True)),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('e'), None, None,
                    ast.Typename(ast.Array(
                        ast.Typename(ast.Array(
                            ast.Typename(ast.Identifier('int32'))),
                                     nullable=True)),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('f'), None, None,
                    ast.Typename(ast.Array(
                        ast.Typename(ast.Identifier('int32')), fixed_size=1),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('g'), None, None,
                    ast.Typename(ast.Array(ast.Typename(
                        ast.Identifier('string'), nullable=True),
                                           fixed_size=1),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('h'), None, None,
                    ast.Typename(ast.Identifier('some_struct'), nullable=True),
                    None),
                ast.StructField(
                    ast.Name('i'), None, None,
                    ast.Typename(ast.Identifier('handle'), nullable=True),
                    None),
                ast.StructField(
                    ast.Name('j'), None, None,
                    ast.Typename(ast.Identifier('handle<data_pipe_consumer>'),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('k'), None, None,
                    ast.Typename(ast.Identifier('handle<data_pipe_producer>'),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('l'), None, None,
                    ast.Typename(ast.Identifier('handle<message_pipe>'),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('m'), None, None,
                    ast.Typename(ast.Identifier('handle<shared_buffer>'),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('n'), None, None,
                    ast.Typename(ast.Receiver(ast.Identifier('some_interface')),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('o'), None, None,
                    ast.Typename(ast.Identifier('handle<platform>'),
                                 nullable=True), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidNullableTypes(self):
    """Tests that invalid nullable types are correctly detected."""
    source1 = """\
        struct MyStruct {
          string?? a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected '\?':\n"
        r" *string\?\? a;$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          handle?<data_pipe_consumer> a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:2: Error: Unexpected '<':\n"
        r" *handle\?<data_pipe_consumer> a;$"):
      parser.Parse(source2, "my_file.mojom")

  def testSimpleUnion(self):
    """Tests a simple .mojom source that just defines a union."""
    source = """\
        module my_module;

        union MyUnion {
          int32 a;
          double b;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Union(
                ast.Name('MyUnion'), None,
                ast.UnionBody([
                    ast.UnionField(ast.Name('a'), None, None,
                                   ast.Typename(ast.Identifier('int32'))),
                    ast.UnionField(ast.Name('b'), None, None,
                                   ast.Typename(ast.Identifier('double')))
                ]))
        ])
    actual = parser.Parse(source, "my_file.mojom")
    self.assertEquals(actual, expected)

  def testUnionWithOrdinals(self):
    """Test that ordinals are assigned to fields."""
    source = """\
        module my_module;

        union MyUnion {
          int32 a @10;
          double b @30;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Union(
                ast.Name('MyUnion'), None,
                ast.UnionBody([
                    ast.UnionField(ast.Name('a'), None, ast.Ordinal(10),
                                   ast.Typename(ast.Identifier('int32'))),
                    ast.UnionField(ast.Name('b'), None, ast.Ordinal(30),
                                   ast.Typename(ast.Identifier('double')))
                ]))
        ])
    actual = parser.Parse(source, "my_file.mojom")
    self.assertEquals(actual, expected)

  def testUnionWithStructMembers(self):
    """Test that struct members are accepted."""
    source = """\
        module my_module;

        union MyUnion {
          SomeStruct s;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Union(
                ast.Name('MyUnion'), None,
                ast.UnionBody([
                    ast.UnionField(ast.Name('s'), None, None,
                                   ast.Typename(ast.Identifier('SomeStruct')))
                ]))
        ])
    actual = parser.Parse(source, "my_file.mojom")
    self.assertEquals(actual, expected)

  def testUnionWithArrayMember(self):
    """Test that array members are accepted."""
    source = """\
        module my_module;

        union MyUnion {
          array<int32> a;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Union(
                ast.Name('MyUnion'), None,
                ast.UnionBody([
                    ast.UnionField(
                        ast.Name('a'), None, None,
                        ast.Typename(
                            ast.Array(ast.Typename(ast.Identifier('int32')))))
                ]))
        ])
    actual = parser.Parse(source, "my_file.mojom")
    self.assertEquals(actual, expected)

  def testUnionWithMapMember(self):
    """Test that map members are accepted."""
    source = """\
        module my_module;

        union MyUnion {
          map<int32, string> m;
        };
        """
    expected = ast.Mojom(ast.Module(
        ast.Identifier('my_module'), None), ast.ImportList(), [
            ast.Union(
                ast.Name('MyUnion'), None,
                ast.UnionBody([
                    ast.UnionField(
                        ast.Name('m'), None, None,
                        ast.Typename(
                            ast.Map(ast.Identifier('int32'),
                                    ast.Typename(ast.Identifier('string')))))
                ]))
        ])
    actual = parser.Parse(source, "my_file.mojom")
    self.assertEquals(actual, expected)

  def testUnionDisallowNestedStruct(self):
    """Tests that structs cannot be nested in unions."""
    source = """\
        module my_module;

        union MyUnion {
          struct MyStruct {
            int32 a;
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:4: Error: Unexpected 'struct':\n"
        r" *struct MyStruct {$"):
      parser.Parse(source, "my_file.mojom")

  def testUnionDisallowNestedInterfaces(self):
    """Tests that interfaces cannot be nested in unions."""
    source = """\
        module my_module;

        union MyUnion {
          interface MyInterface {
            MyMethod(int32 a);
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:4: Error: Unexpected 'interface':\n"
        r" *interface MyInterface {$"):
      parser.Parse(source, "my_file.mojom")

  def testUnionDisallowNestedUnion(self):
    """Tests that unions cannot be nested in unions."""
    source = """\
        module my_module;

        union MyUnion {
          union MyOtherUnion {
            int32 a;
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:4: Error: Unexpected 'union':\n"
        r" *union MyOtherUnion {$"):
      parser.Parse(source, "my_file.mojom")

  def testUnionDisallowNestedEnum(self):
    """Tests that enums cannot be nested in unions."""
    source = """\
        module my_module;

        union MyUnion {
          enum MyEnum {
            A,
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError, r"^my_file\.mojom:4: Error: Unexpected 'enum':\n"
        r" *enum MyEnum {$"):
      parser.Parse(source, "my_file.mojom")

  def testValidAssociatedKinds(self):
    """Tests parsing associated interfaces and requests."""
    source1 = """\
        struct MyStruct {
          pending_receiver<MyInterface> a;
          pending_associated_receiver<MyInterface> b;
          pending_receiver<MyInterface>? c;
          pending_associated_receiver<MyInterface>? d;
        };
        """
    expected1 = ast.Mojom(None, ast.ImportList(), [
        ast.Struct(
            ast.Name('MyStruct'), None,
            ast.StructBody([
                ast.StructField(
                    ast.Name('a'), None, None,
                    ast.Typename(ast.Receiver(ast.Identifier('MyInterface'))),
                    None),
                ast.StructField(
                    ast.Name('b'), None, None,
                    ast.Typename(
                        ast.Receiver(ast.Identifier('MyInterface'),
                                     associated=True)), None),
                ast.StructField(
                    ast.Name('c'), None, None,
                    ast.Typename(ast.Receiver(ast.Identifier('MyInterface')),
                                 nullable=True), None),
                ast.StructField(
                    ast.Name('d'), None, None,
                    ast.Typename(ast.Receiver(ast.Identifier('MyInterface'),
                                              associated=True),
                                 nullable=True), None)
            ]))
    ])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    source2 = """\
        interface MyInterface {
          MyMethod(pending_receiver<A> a) =>(pending_associated_receiver<B> b);
        };"""
    expected2 = ast.Mojom(None, ast.ImportList(), [
        ast.Interface(
            ast.Name('MyInterface'), None,
            ast.InterfaceBody(
                ast.Method(
                    ast.Name('MyMethod'), None, None,
                    ast.ParameterList(
                        ast.Parameter(
                            ast.Name('a'), None, None,
                            ast.Typename(ast.Receiver(ast.Identifier('A'))))),
                    ast.ParameterList(
                        ast.Parameter(
                            ast.Name('b'), None, None,
                            ast.Typename(
                                ast.Receiver(ast.Identifier('B'),
                                             associated=True)))))))
    ])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

  def testLexState(self):
    """Tests that the lex state of tokens is attached to AST nodes."""
    source1 = """struct MyStruct {\n  string foo = "hi";\n};"""
    actual = parser.Parse(source1, "my_file.mojom")
    self.assertEquals(1, len(actual.definition_list))
    struct = actual.definition_list[0]
    self.assertIsInstance(struct, ast.Struct)
    self.assertEquals(ast.Location(1, 0), struct.start)
    self.assertEquals(ast.Location(3, 41), struct.end)
    self.assertEquals(ast.Name('MyStruct'), struct.mojom_name)
    self.assertEquals(ast.Location(1, 7), struct.mojom_name.start)
    self.assertEquals(ast.Location(1, 15), struct.mojom_name.end)
    self.assertEquals(1, len(struct.body.items))
    field = struct.body.items[0]
    self.assertEquals(ast.Location(2, 20), field.start)
    self.assertEquals(ast.Location(2, 38), field.end)
    self.assertEquals(ast.Location(2, 20), field.typename.start)
    self.assertEquals(ast.Location(2, 26), field.typename.end)
    self.assertEquals(ast.Location(2, 27), field.mojom_name.start)
    self.assertEquals(ast.Location(2, 30), field.mojom_name.end)
    self.assertEquals(ast.Location(2, 33), field.default_value.start)
    self.assertEquals(ast.Location(2, 37), field.default_value.end)

  def testCommentAttachment(self):
    source1 = """\
        // Before import 1.
        // Can span two lines.

        // Before import 2.
        import "foo.mojom";  // End-of-import.

        // Before struct.
        struct Foo {
          // Before field.
          string field;  // End-of-field.

          // End of struct.
        };

        // End-of-file 1.

        // End-of-file 2.
        """
    tree = parser.Parse(source1, "my_file.mojom", with_comments=True)

    self.assertEquals(1, len(tree.import_list.items))
    self.assertEquals(2, len(tree.import_list.items[0].comments_before))
    self.assertIn('// Before import 1.\n',
                  tree.import_list.items[0].comments_before[0].value)
    self.assertIn('// Can span two lines.',
                  tree.import_list.items[0].comments_before[0].value)
    self.assertEquals('// Before import 2.',
                      tree.import_list.items[0].comments_before[1].value)

    self.assertEquals(1, len(tree.definition_list))
    struct = tree.definition_list[0]
    self.assertEquals(1, len(struct.comments_before))
    self.assertEquals('// Before struct.', struct.comments_before[0].value)
    self.assertEquals(1, len(struct.comments_after))
    self.assertEquals('// End of struct.', struct.comments_after[0].value)
    self.assertEquals(1, len(struct.body.items))

    field = struct.body.items[0]
    self.assertEquals(1, len(field.comments_before))
    self.assertEquals('// Before field.', field.comments_before[0].value)
    self.assertEquals(1, len(field.comments_suffix))
    self.assertEquals('// End-of-field.', field.comments_suffix[0].value)

    self.assertEquals(2, len(tree.comments_after))
    self.assertEquals('// End-of-file 1.', tree.comments_after[0].value)
    self.assertEquals('// End-of-file 2.', tree.comments_after[1].value)

    source2 = "// Comment only.\n// Comment two.\n"
    tree = parser.Parse(source2, "my_file.mojom", with_comments=True)
    self.assertEquals(1, len(tree.comments_after))
    self.assertEquals(source2.strip(), tree.comments_after[0].value)

    source3 = """\
        // Before interface.
        interface Foo {  // End of interface.
          Method();

          // Interface 1.

          // Interface 2.

          // Interface 3.
          [Sync] Foo(
                string a,  // End param a.
                int32 b);  // End param b.

          // Interface 4.
          // Continuation of 4.

          // Interface 5.
        };  // Trailing interface.
        """
    tree = parser.Parse(source3, "my_file.mojom", with_comments=True)
    interface = tree.definition_list[0]
    self.assertEquals(2, len(interface.comments_before))
    self.assertEquals('// Before interface.',
                      interface.comments_before[0].value)
    # This is an odd place to attach the comment, but it is also an odd
    # place to leave a comment.
    self.assertEquals('// End of interface.',
                      interface.comments_before[1].value)
    self.assertEquals(2, interface.start.line)

    meth = interface.body.items[1]
    self.assertEquals(3, len(meth.comments_before))
    self.assertEquals('// Interface 1.', meth.comments_before[0].value)
    self.assertEquals('// Interface 2.', meth.comments_before[1].value)
    self.assertEquals('// Interface 3.', meth.comments_before[2].value)
    self.assertEquals(1, len(meth.parameter_list.items[0].comments_suffix))
    self.assertEquals('// End param a.',
                      meth.parameter_list.items[0].comments_suffix[0].value)
    self.assertEquals(1, len(meth.parameter_list.items[1].comments_suffix))
    self.assertEquals('// End param b.',
                      meth.parameter_list.items[1].comments_suffix[0].value)

    self.assertEquals(3, len(interface.comments_after))
    self.assertIn('// Interface 4.\n', interface.comments_after[0].value)
    self.assertIn('// Continuation of 4.', interface.comments_after[0].value)
    self.assertEquals('// Interface 5.', interface.comments_after[1].value)
    self.assertEquals('// Trailing interface.',
                      interface.comments_after[2].value)

if __name__ == "__main__":
  unittest.main()
