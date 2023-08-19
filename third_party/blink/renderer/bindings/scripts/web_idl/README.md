# Web IDL compiler (web_idl package)

[TOC]

## What's web_idl?

Python package
[`web_idl`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/)
is the core part of Web IDL compiler of Blink.
[`build_web_idl_database.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/build_web_idl_database.py)
is the driver script, which takes a set of ASTs of \*.idl files as inputs and
produces a database(-ish) pickle file that contains all information about
spec-author defined types (IDL interface, IDL dictionary, etc.). The database
file will be used as the input of
[Blink-V8 bindings code generator](../bind_gen/README.md).

## Quick walk in web_idl_database.pickle

The following snippet of shell commands and Python scripts demonstrates how you
can build and use the database file (`web_idl_database.pickle`) and what you
can do with the database file just as an example.

```shell
# Build the database file `web_idl_database.pickle`.
# output: out/Default/gen/third_party/blink/renderer/bindings/web_idl_database.pickle
$ autoninja -C out/Default web_idl_database

# Play with the produced database.
$ PYTHONPATH=third_party/blink/renderer/bindings/scripts:$PYTHONPATH python3
>>> import web_idl
>>> web_idl_database_path = 'out/Default/gen/third_party/blink/renderer/bindings/web_idl_database.pickle'
>>> web_idl_database = web_idl.Database.read_from_file(web_idl_database_path)

# Print all IDL attributes whose type is boolean (but not nullable boolean).
>>> for interface in web_idl_database.interfaces:
...   for attribute in interface.attributes:
...     if attribute.idl_type.is_boolean:
...       print("{}.{}".format(interface.identifier, attribute.identifier))

# Print all IDL dictionary members which are required members.
>>> for dictionary in web_idl_database.dictionaries:
...   for member in dictionary.own_members:
...     if member.is_required:
...       print("{}.{}".format(dictionary.identifier, member.identifier))

# Print API references of IDL dictionary class.
>>> help(web_idl.Dictionary)

# Print API references of a certain object (Window interface in this case).
>>> window = web_idl_database.find('Window')
>>> help(window)
```

## Preprocessing of \*.idl files

\*.idl files are preprocessed by
[`collect_idl_files.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/collect_idl_files.py)
before the main IDL compiler starts processing. There are two major purposes.

1. To transform the text files (\*.idl) into ASTs (abstract syntax trees).
The \*.idl files are parsed by
[`//tools/idl_parser/`](https://source.chromium.org/chromium/chromium/src/+/main:tools/idl_parser/),
which is a Blink-independent IDL parser. The resulting ASTs are represented
as trees of
[`idl_parser.idl_node.IDLNode`](https://source.chromium.org/chromium/chromium/src/+/main:tools/idl_parser/idl_node.py?q=class:%5EIDLNode$&ss=chromium).

2. To group the ASTs by component. The resulting ASTs are grouped by component
(core/, modules/, etc.) into
[`web_idl.AstGroup`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/ast_group.py?q=class:%5EAstGroup$&ss=chromium).

The `web_idl.AstGroup`s are saved in intermediate \*.pickle files. See also
`collect_idl_files` rules in `BUILD.gn`.

## Overview of compilation flow of \*.idl files

The goal of Web IDL compilation is to produce Python objects that represent
Web IDL definitions and to save them into a single \*.pickle file
(`web_idl_database.pickle`). These Python objects are designed to be immutable
as much as possible (they have no setter methods, and their getter methods
return `tuple`s rather than `list`s, for example).

Very roughly speaking, the compilation flow of Web IDL files are as below.

    AST nodes ==> IRs (mutable) ==> public (final immutable) objects

AST nodes are represented in `idl_parser.idl_node.IDLNode`, IR objects are
represented in `web_idl.Interface.IR`, `web_idl.Dictionary.IR`, etc., and
public objects are represented in `web_idl.Interface`, `web_idl.Dictionary`,
etc.

### Step 1. Read AST nodes and build IRs

[`_IRBuilder.build_top_level_def`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/ir_builder.py?q=function:%5E_IRBuilder.build_top_level_def$&ss=chromium)
takes an AST node of a top-level definition and returns an IR. The structure of
AST nodes is very specific to each Web IDL definition, so each
`_IRBuilder._build_xxxx` is also specific to Web IDL definition.

In `_IRBuilder`, two factory functions are used: `_create_ref_to_idl_def` and
`_idl_type_factory`.

`_create_ref_to_idl_def` is a function that creates a `web_idl.RefById`, which
is a placeholder of a (final immutable) public object. In the middle of
compilation, we don't have any public object yet, so we need a placeholder
which will be later resolved to a reference to a public object. For example,
when `obj.attr` is a `RefById` to "my_object", then `obj.attr.foo` behaves the
same as `x.foo` where `x` is a public object with identifier "my_object".

`_idl_type_factory` is a factory object that creates a (subclass of)
[`web_idl.IdlType`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/idl_type.py?q=class:%5EIdlType$&ss=chromium).
Unlike IRs, `web_idl.IdlType` is designed to be (almost) immutable (although
there are some exceptions) so there is no `web_idl.IdlType.IR`.

These two factories are used in order to track all the instances of
`web_idl.RefById` and `web_idl.IdlType` respectively (each factory has a
`for_each` method). For example, the IDL compiler replaces placeholders with
(final immutable) public objects. The IDL compiler also creates `web_idl.Union`
objects, which represent Blink's IDL union objects, based on all instances of
`web_idl.IdlType`.

### Step 2. Apply changes on IRs in each compilation phase

The IDL compiler has multiple "compilation phases" in order to process the IDL
definitions step by step. For example, (1) apply a partial interface's
[RuntimeEnabled] to each member, and then (2) copy the partial interface's
members to the primary interface definition; these two steps make interface
members have appropriate extended attributes propagated from partial interface
definitions. This is just an example of two compilation phases; there actually
exist many compilation phases.

```webidl
interface MyInterface {
  attribute DOMString name;
};

[RuntimeEnabled=Foo] partial interface MyInterface {
  attribute DOMString nickname;
};

// The IDL compiler turns the above definitions into the following in two
// compilation phases.
interface MyInterface {
  attribute DOMString name;
  [RuntimeEnabled=Foo] attribute DOMString nickname;
};
```

At each compilation phase, a set of new IRs is produced from the current IRs.
The new IRs are saved in an
[`IRMap`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/ir_map.py?q=class:%5EIRMap$&ss=chromium)
associated with the compilation phase number. By using the `IRMap`, a
compilation phase runs in the following way.

1. Reads IRs from the `IRMap` and the current compilation phase number. These
IRs are called `old_ir` in `IdlCompiler`.
1. Increments the compilation phase number.
1. Creates copies of the old IRs (so that we don't overwrite the existing IRs).
The copies are called `new_ir` in `IdlCompiler`.
1. Makes changes to the new IRs. The new IRs have a new state.
1. Saves the new IRs in the `IRMap` associated with the (incremented)
compilation phase number.

A compilation phase consists of these steps, and the IDL compiler runs as many
compilation phases as needed. You can see the list of compilation phases at
[`IdlCompiler.build_database`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/idl_compiler.py?q=function:%5EIdlCompiler.build_database$&ss=chromium).

The initial set of IRs is constructed by
[`_IRBuilder.build_top_level_def`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/ir_builder.py?q=function:%5E_IRBuilder.build_top_level_def$&ss=chromium)
(as explained above) and registered in the `IRMap`.

### Step 3. Create the public objects

At the very last compilation phase, the public objects (which are final,
immutable, and exposed to users of this `web_idl` module) are constructed from
the last IRs, and registered to
[`web_idl.Database`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/database.py?q=class:%5EDatabase$&ss=chromium).
The `Database` is saved as a pickle file, whose name by default is
`out/Default/gen/third_party/blink/renderer/bindings/web_idl_database.pickle`.
It can be read via
[`web_idl.Database.read_from_file`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/database.py?q=function:%5EDatabase.read_from_file$&ss=chromium).
