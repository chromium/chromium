The **IDL compiler** or **bindings generator** [transcompiles](http://en.wikipedia.org/wiki/Transcompiler) [Web IDL](https://sites.google.com/a/chromium.org/dev/blink/webidl) to C++ code, specifically [bindings](http://en.wikipedia.org/wiki/Language_binding) between V8 (the [JavaScript engine](http://en.wikipedia.org/wiki/JavaScript_engine)) and Blink. That is, when an attribute or method in a [Web IDL interface](https://sites.google.com/a/chromium.org/dev/developers/web-idl-interfaces) is used from JavaScript, V8 calls the bindings code, which calls Blink code.

As of early 2022, there are almost 2,000 IDL files, about 5,000 attributes and 3,000 operations. This bindings code is highly repetitive, primarily passing values (arguments and return values) between V8 and Blink and doing various conversions and checks (e.g., type checking), hence it is mostly machine-generated.

This is complicated by numerous parameters and special cases, due to either wanting to automatically generate repetitive code (so that the Blink implementation is simpler), supporting quirks of Web interfaces, or Blink implementation details.

The compiler is in [renderer/bindings/scripts](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/), and bindings are generated as part of the build.

**The rest of this document is obsolete.**
See
[renderer/bindings/scripts/web_idl/README.md](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/README.md)
and
[renderer/bindings/scripts/bind_gen/README.md](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/bind_gen/README.md)
about the design of the current bindings generator.

TODO(yukishiino): Update this document.

## Overall structure
The compiler is factored into a pipeline, and the steps further divided into separate Python modules. While there are a number of components, each is simple and well-defined, and generally only one or two must be considered at a time.

There are complicated issues with global information such as dependencies (see below), but initially it is sufficient to consider compiling a single file.

The overall structure is standard for [compilers](http://en.wikipedia.org/wiki/Compiler) (see [compiler construction](http://en.wikipedia.org/wiki/Compiler_construction)): a front end reads in input (IDL), yielding an [intermediate representation](http://en.wikipedia.org/wiki/Intermediate_representation) (IR), and the back end takes the IR and writes out output (C++).

```
IDL (Foo.idl)  --front end-->  IR  --back end-->  C++ (V8Foo.h, V8Foo.cpp)
```

The flow is a (per file) [pipeline](http://en.wikipedia.org/wiki/Pipeline_(software)): information flows one way, and, other than dependencies and global information, files are processed independently. In detail:

```
Front end: IDL  --lexer-->  tokens  --parser-->  AST  --constructor-->  IR
Back end:  IR  --logic-->  context  --template processor-->  C++
```

In terms of data types, objects, and modules, this is:

```
IdlCompiler: idl_filename  --IdlReader-->  IdlDefinitions  --CodeGeneratorV8-->  header_text, cpp_text

IdlReader:   idl_filename  --BlinkIDLLexer--> (internal) --BlinkIDLParser-->  IDLNode
               --idl_definitions_builder-->  IdlDefinitions

CodeGeneratorV8:  IdlDefinitions  --(extract member)-->  IdlInterface  --v8_*.py-->  dict
                    --jinja2-->  header_text, cpp_text
```

One can trace the processing of each element of IDL input text through the pipeline, essentially 1-to-1:
*   Spec: [Interfaces](https://webidl.spec.whatwg.org/#idl-interfaces) (production rule: [Interface](https://webidl.spec.whatwg.org/#prod-Interface))
*   Input IDL text is parsed by the `p_Interface` rule in `idl_parser.IDLParser` (base parser), producing an `IDLNode` (AST) named `'Interface'`.
*   An `IdlInterface` is constructed from this tree in `idl_definitions_builder`
*   Python logic produces a dict (Jinja context) via the `v8_interface.interface_context` function
*   The context is rendered to `.cpp/.h` output text by Jinja, from the templates `interface.cpp` (derived from `interface_base.cpp`) and `interface.h`

The boundary between the stages of the pipeline can move, primarily in the CG between the context and the template (depending on logic complexity: can it fit in the template, or should it be a Python function?). Changing the boundary between the FE and the CG is possible (e.g., moving "find indexed property getter" into the `IdlInterface` constructor, instead of the CG `v8_interface` module), but rare. In principle the constructor could be entirely integrated into the parser (have the yacc actions construct the object directly, instead of an AST), but this is unlikely, as it would require a significant amount of work for a relatively minor simplification.

Note that filenames, not IDL text, are what is input to the reader. This is fundamentally because the compiler and the parser assume they are reading in files (hence take a filename parameter, e.g. for reporting errors). Further, due to dependencies, processing one IDL file may require reading others (hence going out to the file system). This could be abstracted to taking in IDL text and passing interface names rather than filenames internally, but this isn't necessary at present.

The front end is familiar ([regex](http://en.wikipedia.org/wiki/Regular_expression) [lexer](http://en.wikipedia.org/wiki/Lexical_analysis), [LALR parser](http://en.wikipedia.org/wiki/LALR_parser), and object constructor). The lexer-parser uses [PLY (Python Lex-Yacc)](http://www.dabeaz.com/ply/), a Python implementation of [lex](http://en.wikipedia.org/wiki/Lex_(software)) and [yacc](http://en.wikipedia.org/wiki/Yacc); the constructor is lengthy but straightforward.

However, the back end ([code generator](http://en.wikipedia.org/wiki/Code_generation_(compiler))) differs from compilers that target machine code: since it is a [source-to-source compiler](http://en.wikipedia.org/wiki/Source-to-source_compiler), the back end outputs C++ (source code, not machine code). Thus we use a [template processor](http://en.wikipedia.org/wiki/Template_processor) to produce output; recall that _templating_ (writing formatted _output_) is complementary to _parsing_ (reading formatted _input_). The template processor is [Jinja](https://sites.google.com/a/chromium.org/dev/developers/jinja), which is used in several places in Chromium. Given an interface, containing various attributes and methods, we fill in a template for the corresponding C++ class and each attribute and method.

Further, the input language is declarative (the 'D' in IDL), so no optimizations of input code are necessary (there is no _middle end_): it's just filling in a template. The back end is itself divided into the templates themselves, and the Python code that fills in the templates (produces the _context_). There is also no separate [_semantic analysis_](http://en.wikipedia.org/wiki/Semantic_analysis_(compilers)) step, except for validation of extended attributes (see below): the code generator assumes types are valid, and errors show up when the resulting C++ code fails to compile. This avoids the complexity and time cost of either a separate validation step, or of type-checking in the code generator, at the cost of typos showing up in compile failures instead of at IDL compile time. Notably, there is no type checking or name binding, since identifiers are assumed to refer to Web IDL interfaces (C++ classes) and the Web IDL namespace is global, and there is no assignment, since Web IDL is declarative.

## Code
Code-wise, the top-level file [`idl_compiler.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/idl_compiler.py) imports two modules: [`idl_reader`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/idl_reader.py) for the front end, [`code_generator_v8`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/code_generator_v8.py) for the back end. Each of these is used to create an object (`IdlReader` and `CodeGeneratorV8`), which handles the library initialization (PLY or Jinja, which are slow, and thus reused if running multiple times during one run) and global information. The objects are then called one time each, for the IDL --> IR and IR --> C++ steps, respectively. By contrast, `run_bindings_tests.py` creates these objects as well, then calls them multiple times, once for each test file. Note that in the actual build, compilation is parallelized, which is why only one file is compiled per process, and there is a pre-caching step which significantly speeds up library initialization.

The code is mostly functional, except for a few module-level variables in the code generator (discussed below) for simplicity, and some objects used for initialization.

The main classes are as follows, with the module in which they are defined. Note that the relations are primarily composition, with inheritance significantly used for the Blink-specific lexer and parser. `IdlCompiler, IdlReader,` and `CodeGeneratorV8` are expected to be used as singletons, and are just classes for initialization (avoids expensive re-initialization, and interface-wise separates initialization from execution). `IdlDefinitions` contains objects of a few other classes (for more minor data), and there is a bit of complex OO code in [`idl_definitions`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/idl_definitions.py) to simplify [typedef](https://webidl.spec.whatwg.org/#idl-typedefs) resolution.

```
IdlCompiler  :: idl_compiler
    IdlReader  ::  idl_reader
        BlinkIDLParser < IDLParser
            BlinkIDLLexer < IDLLexer
        IDLExtendedAttributeValidator
    CodeGeneratorV8  :: code_generator_v8
```
```
IdlDefinitions  ::  idl_definitions
    IdlInterface
        IdlAttribute
        IdlConstant
        IdlOperation
            IdlArgument
```

## Front end
The front end is structured as a lexer --> parser --> object constructor pipeline:

Front end: `IDL  --lexer-->  tokens  --parser-->  AST  --constructor-->  IR`

There are two other steps:
*   extended attribute validation
*   dependency resolution

The top-level module for the front end is [`idl_reader`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/idl_reader.py). This implements a class, `IdlReader`, whose constructor also constructs a lexer, parser, validator, and resolver. `IdlReader` implements a method to construct an IR object (`IdlDefinitions`) from an IDL filename. Thus to convert IDL to IR one instantiates a reader, then call its method to read an IDL file.

The lexer-parser uses [PLY (Python Lex-Yacc)](http://www.dabeaz.com/ply/). In fact, the lexer and parser for the Blink IDL dialect of Web IDL derive from a base lexer and base parser for standard Web IDL (in [tools/idl_parser](https://source.chromium.org/chromium/chromium/src/+/main:tools/idl_parser)), and thus only need to include deviations from the standard. The lexical syntax of Blink IDL is standard (though the base lexer is slightly non-standard), so the Blink IDL lexer is very small and slated for removal (Bug [345137](https://code.google.com/p/chromium/issues/detail?id=345137)). The phrase syntax is slightly non-standard (primarily in extended attributes) and expected to stay this way, as extended attributes are implementation-dependent and the deviations are useful (see [Blink IDL: syntax](http://www.chromium.org/blink/webidl#TOC-Syntax)). We thus say that the base lexer/parser + Blink lexer/parser form 1 lexer and 1.1 parser (base + derived).

The base lexer class, defined in [`idl_lexer`](https://source.chromium.org/chromium/chromium/src/+/main:tools/idl_parser/idl_lexer.py), is straightforward and short: it is just a list of regular expressions and keywords, wrapped in a class, `IDLLexer`; a lexer (object) itself is an instance of this class. There is some minor complexity with error handling (correct line count) and methods to assist with derived classes, but it's quite simple. The derived lexer class is very simple: it's a class, `BlinkIDLLexer`, which derives from the base class. The only complexity is adding a method to remove a token class from the base class, and to remove comments (base lexer is non-standard here: standard lexer does not produce `COMMENT` tokens).

The base parser class, defined in [`idl_parser`](https://source.chromium.org/chromium/chromium/src/+/main:tools/idl_parser/idl_parser.py), is considerably longer, but consists almost entirely of production rules in (a form of) [BNF](http://en.wikipedia.org/wiki/Backus%E2%80%93Naur_Form), together with yacc _actions_ that build an [abstract syntax tree](http://en.wikipedia.org/wiki/Abstract_syntax_tree) (AST, syntax tree). Recall that yacc _traverses_ the [concrete syntax tree](http://en.wikipedia.org/wiki/Concrete_syntax_tree) (CST, parse tree) and can take whatever actions it chooses; in this case it generates an AST, though it could also generate the IR directly. See [`blink_idl_parser`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/blink_idl_parser.py) for a detailed explanation of the PLY yacc syntax and `IDLParser` methods used to construct the AST. The grammar is directly from the [Web IDL grammar](https://webidl.spec.whatwg.org/#idl-grammar), which is an [LL(1) grammar](http://en.wikipedia.org/wiki/LL_grammar); the Blink IDL rules are exactly the deviations from the standard grammar, or overrides to irregularities in the base parser. A parser object is an instance of a class, `IDLParser`, from which `BlinkIDLParser` is derived. The parser constructor takes a lexer as an argument, and one passes the corresponding lexer.

The definitions classes, defined in [idl_definitions](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/idl_definitions.py), primarily consist of constructors, which take the AST and generate an intermediate representation (IR), in this case an object of type `IdlDefinitions` and contained objects for individual definitions and definition's members.

The classes are as follows (mostly composition, one case of inheritance); a few internal-use only classes are not shown:

```
IdlDefinitions
    IdlInterface
        IdlAttribute
        IdlConstant
        IdlOperation
            IdlArgument
    IdlException < IdlInterface
    IdlCallbackFunction
    IdlEnum
```

After reading an IDL file (producing an IR), the reader has two optional additional steps (these are run during the build, but aren't necessary for simpler use, such as compiling a test file without dependencies): validation and dependency resolution.

### Extended attribute validation
The front end largely does not do [semantic analysis](http://en.wikipedia.org/wiki/Semantic_analysis_%28compilers%29), as semantic errors (primarily name errors) are largely caught by the build visibly failing, either during the IDL compile or during the C++ compile (name lookup errors). Notably, there is no symbol table or name binding in the front end: each name is simply looked up on use (e.g., type names), or passed through to the output (e.g., attribute names), and catch errors by the lookup failing or the generated code failing to compile, respectively.

However, extended attributes are validated, both the keys and values, based on the list in [`IDLExtendedAttributes.txt`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/IDLExtendedAttributes.txt). This is done because invalid extended attributes are _ignored_ by the compiler, specifically the code generator, and thus errors are easy to miss. The Python code checks for specific extended attributes, so errors just result in the attribute not being found; for example, writing `[EnforecRange]` for `[EnforceRange]` would otherwise silently result in the range not being enforced. This is not perfect: extended attributes may be valid but used incorrectly (e.g., specified on an attribute when they only apply to methods), which is not caught; this is however a minor problem.

This is done in the front end since that is the proper place for semantic analysis, and simplifies the code generator.

### Dependency resolution
IDL interface definitions have two types of dependencies:
*   An IDL interface definition can be spread across multiple files, via partial interfaces and (in effect) by `implements` (interface inheritance). We call these _dependency interface definitions, dependency interfaces,_ or just _dependencies_.
*   An IDL interface may use other interfaces as interface types: e.g., `interface Foo { attribute Bar bar; };` We call these _referenced interfaces_.

The dependency resolution phase consists of merging the dependencies into the main interface definition, and adding referenced interfaces to the list of all interfaces (available to the code generator). These dependencies are computed during the _Global information_ stage; see below.

Members defined in partial interfaces and in implemented interfaces are simply appended to the list of members of the main interface, with a few caveats (below). Formally the difference between partial interfaces and implemented interfaces is that a partial interface is (external) type extension (it is part of and modifies the type, but is just defined separately), and a partial interface is associated with a _single_ main interface that it extends; while implemented interfaces are _interface inheritance_ (providing multiple inheritance in IDL) and _several_ interfaces can implement a given (implemented) interface. This difference is only for IDL: in JavaScript these are both just exposed as properties on the object implementing the interface; and thus we mostly don't distinguish them in the code generator either, just merging them into one list.

The subtleties are:
*   Members defined in partial interfaces are treated differently by the code generator: they are assumed to be implemented as static members in separate classes, not as methods on the object itself. This is because partial interfaces are usually for optional or specialized parts of a basic interface, which might only be used by subtypes, and thus we do not want to clutter the main Blink C++ class with these.
*   Some extended attributes are transferred from the dependency interface definition to its members, because otherwise there is no way to apply the attributes to "just the members in this definition", due to merging. These are extended attributes for turning off a dependency interface, namely: `[Conditional], [PerContextEnabled],` and `[RuntimeEnabled].`
*   Interface-level `[TypeChecking]` extended attributes are also recognized on partial interfaces as a convenience. Its value is also transferred to its members, appending it to any `[TypeChecking]` values that the member already declares.

Also, note that the compiler currently does not do significant type introspection of referenced interfaces: it mostly just uses certain global information about the interface (is it a callback interface?, `[ImplementedAs],` etc.). The only significant use of type introspection is in `[PutForwards],` where generating the code for an attribute with `[PutForwards]` requires looking up the referenced attribute in another interface. Type introspection may increase in future, to simplify complex cases and reduce hard-coding, but for now is in very limited use.</div>

## Back end (code generator)
The back end (code generator, CG) is where the bulk of the complexity is. The structure is simple: the Python V8 CG modules (`v8_*`) generate a dictionary called the **context**, which is passed to Jinja, which uses this dict to fill in the appropriate templates ([bindings/templates](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/templates/)/*.{cpp,h}). The modules correspond to the class structure in IR, with the following exceptions:
*   the CG uses “method” where the front end uses “operation” (the term in the spec), since “method” is a more standard term;
*   constants are included in `v8_interface`, because very simple; and
*   arguments are included in `v8_methods`, because they are closely related to methods.

Recall the overall flow of the CG:
```
CodeGeneratorV8:  IdlDefinitions  --(extract member)-->  IdlInterface  --v8_*.py-->  dict
                    --jinja2-->  header_text, cpp_text
```

The class structure of the IR, and corresponding Python module that processes it (in _italics_ if included in a previously mentioned module), are:
```
IdlInterface  ::  v8_interface
    IdlAttribute  ::  v8_attributes
    IdlConstant   ::  _v8_interface_
    IdlOperation  ::  v8_methods
        IdlArgument  ::  _v8_methods_
```
The CG modules each have a top-level `*_context` function_,_ which takes an IR object (`Idl*`) as a parameter and returns a context (`dict`), looping through contained IR objects as necessary.

Note that the context is nested – the context for an interfaces contains a list of contexts for the members: attributes, constants, and methods. The context for a given member is used throughout the corresponding template, generally to generate several methods; e.g., in `attributes.cc.tmpl` there are macros for getter, getter callback, setter, and setter callback, which are called from a loop in `interface_base.cpp`. The context for members is _also_ used at the overall interface level, notably for DOM configuration.

## Style
[`v8_attributes.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/v8_attributes.py) (Python) and [`attributes.cc.tmpl`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/templates/attributes.cc.tmpl) (Jinja) are a good guide to style: the getter and setter methods themselves are quite complex, while the callbacks are simple.

See [Jinja: Style](https://sites.google.com/a/chromium.org/dev/developers/jinja#TOC-Style) for general Jinja style tips; below is bindings generator specific guidance.

### Jinja
If nesting macros (so macros that call macros), use top-down order: top-level macro, followed by macros it calls. This is most notable in [`methods.cc.tmpl`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/templates/methods.cc.tmpl), where generating a method also requires generating code for the arguments, which comes after.

### Python
Assertions should _not_ be used for validating input, and this includes IDL files: if an IDL file is invalid (e.g., it uses an extended attribute improperly), raise an exception explicitly. Assertions can be used for validating internal logic, but in practice this is rare in the code generator; see [Using Assertions Effectively](https://wiki.python.org/moin/UsingAssertionsEffectively).

Deviations from the "One dictionary display per context" rule:
*   If the presence of an extended attribute has side effects (namely includes), this code comes before the display.
*   An early return after the main display, and then additional processing afterwards, occurs for attributes, where certain attributes (constructors, custom attributes) have limited processing, and the side effects of running the additional context-generation code (extra includes) are not desired.
*   Similarly, the getter and setter logic for attributes is relative complicated, and depends on previously computed variables, and thus this part of the context is produced in other functions, then added to the main dictionary via `dict.update()`.

**No extended attributes in templates:** A key point for the CG is that the raw extended attributes are _not_ passed to the Jinja templates, and literal extended attribute names accordingly do not appear in templates. Instead, extended attributes are all passed via some variable in the context, either a content variable or a (boolean) control variable (e.g., `deprecate_as` for the `[DeprecateAs]` extended attribute and `is_reflect` for the `[Reflect]` extended attribute (this is only boolean since the value is used elsewhere, in the function for the getter/setter name)). This is primarily for consistency: in some cases variables _must_ be used, notably if the extended attribute requires a side effect (includes), and thus putting all literal references to extended attributes in the Python logic keeps them together.

**Includes:** The CG code is primarily functional, with one significant exception: _includes_ (for the `.cpp` file; the `.h` includes are simple). Includes are added to the `.cpp` file throughout the CG (as needed by various features), and account for a significant amount of the code bulk. This is handled by considering includes as a [side effect](http://en.wikipedia.org/wiki/Side_effect_(computer_science)), and storing them in a global variable (or rather a module level variable, as Python modules are [singletons](http://en.wikipedia.org/wiki/Singleton_pattern)), specifically in `v8_global`. Alternatives include: being purely functional, and returning the includes from the functions, which makes the calls and returns bulky (due to all the passing backwards and forwards); or having all CG functions be methods of some object (`CodeGeneratorV8`), which adds a large amount of OO boilerplate. The only complexity of using module variables is that one needs to import it into each module, and must clear the includes if compiling multiple files in a single Python run (and can't compile multiple files concurrently in a single Python processes). This latter is ok because compilation is parallelized by distributing across separate processes, not by compiling multiple files in a single process.

Note that the context being computed is _not_ a module-wide variable: for attributes, modules, and arguments, a context is computed for each of these (functionally), though for an interface only a single context is computed. In rare cases the context is also passed into functions as an argument (when later values depend on earlier computed values), which is primarily to flatten the module: it's clearer to pass explicitly, instead of having a nested function.

There are a few other module-level variables for the IDL global context, which are write-once or update-only: `v8_globals.interfaces`, for referenced interfaces, and a few in `v8_types`, for global and local type information (e.g., `[ImplementedAs]`, enumerations). These are set at `CodeGeneratorV8` construction (from `interfaces_info`), which is write-once, or `CodeGeneratorV8.generate_code` invocation (from `definitions`), which is update-only (can add some new enumerations, for instance, but shouldn't need or refer to any from other files).

**Template rendering:** the context is then passed into a Jinja template, via the [`Template.render`](http://jinja.pocoo.org/docs/api/#jinja2.Template.render) method, which returns the generated code.

## Global information
Global information is computed by a pre-processing step, dependency resolution and merging is done by the front end, and global information is used by the code generator in limited places.

`FIXME`

## Dependencies
`FIXME`

## Goals
Key goals of the compiler are as follows (in descending order of importance); these primarily affect the code generator:
*   **Correctness** (including **security**) and **performance** of generated object code (speed, memory usage, size), after compiling C++ to object code
*   **Simplicity** of compiler code itself, particularly code generator
*   **Build performance** (see [IDL build: performance](http://www.chromium.org/developers/design-documents/idl-build#TOC-Performance) and [Performance](http://www.chromium.org/developers/design-documents/idl-compiler#TOC-Performance), below)

Note that **legibility** of generated C++ source code is secondary to legibility of the code generator, as the CG is what is developed and reviewed, though the generated code should be legible if this does not conflict with other goals. As a rule, simple (one-line) changes to CG that simplify the generated source code are ok, but more complex changes that are only for source code legibility (that have no effect on object code) should be avoided. If you rely on the compiler (e.g., for dead code elimination), please write a comment to that effect.

Since the CG is complex enough as it is, a key part of the design is moving complexity _out_ of the code generator, notably to the front end (building an easy-to-use IR) and to the global information step.

## Alternatives
Fundamentally, why do we have our own compiler at all? Can't we reuse an existing program?

The answer is: we're reusing as much code as possible (lexer/parser and template library, and indeed a lexer/parser for the Web IDL standard), but we need custom code for 3 purposes:
*   Blink IDL dialect, which differs from Web IDL standard: we inherit from a standard parser, so this is small, but we need our own.
*   IR constructor: the AST output from the base parser is hard to use directly (you need to walk a tree), so we construct an easy-to-use object so the CG can have simple reading code (instead of walking the tree itself).
*   V8 code generator: we need to generate very specific C++ code to bind to V8 and implement Web IDL, with many options and special cases.

Of these, the V8 CG is the main reason; Blink IDL dialect and the IR constructor exist to reduce complexity of the CG.

### Why not C++?
The compiler is written in Python, rather than C++, because hackability (ease of understanding and modification) is more important than build speed, and build speed is good enough.

A high-level language like Python is the usual choice for such tasks, and Python is the standard high-level language in Chromium: other languages (except JavaScript) are not widely used in Chromium, and JavaScript offers no advantages over Python.

Also, C++ code that generates C++ code risks being very confusing. In principle one could imagine writing the CG in template metaprogramming, but that would likely be hard to understand.

In principle C/C++ code may be used in the front end (lexer, parser, and constructor), particularly as the lexer and parser can be generated by lex-yacc, but the code generator is expected to be in Python.

### Why not SWIG?
We can't use SWIG because there's in fact very little overlap. Essentially we use Jinja templates+Python logic for the backend, rather than SWIG templates.

While SWIG is a general "high-level language to C++ bindings generator", and thus seems a natural fit, it doesn't fill our needs. We need a front end because we're taking IDL files as input, and we need a code generator that supports all the specific features that Web IDL and Blink require: Web IDL has different semantics from JavaScript generally, and Blink has many IDL extended attributes and other implementation details. Further, there is a prototype V8 backend for SWIG, but it is incomplete (the JavaScript backend primarily targets JavaScriptCore). In principle we could have a compiler whose back end output SWIG templates, complete the general V8 backend for SWIG, and then modify this or fork it to make a Web IDL to V8 backend, customized for Blink. As this description makes clear, that involves significant complexity and few advantages over generating the C++ code directly, using Jinja instead of SWIG.

### Why not another lexer/parser or template processor?
PLY (built on lex-yacc) and Jinja are standard and widely-used libraries for these tasks, and are used elsewhere in Chromium. We're open to other libraries if they are significantly superior (and we switch throughout Chromium), but these libraries are fit for purpose.

## Performance
_For overall IDL build performance, see: [IDL build: Performance](http://www.chromium.org/developers/design-documents/idl-build#TOC-Performance)_

The bindings are generated by running a separate compiler process for each interface (main IDL file), for parallelization and simplicity of build. This section discusses optimization of individual IDL compiler runs.

For compilation of an individual IDL file, the key factor in startup time, both Python startup time, and initialization time of libraries. This is because most IDL files are quite short and simple, so processing time (variable cost) is short relative to startup time (fixed cost). Currently (March 2014) the average sequential compilation time of a single IDL file on a fast Linux workstation is ~80 milliseconds, compared to Python startup time of ~4 milliseconds (skipping import of `site` with `-S`).

Naively, the key limiting factor is initialization time of the libraries (PLY and Jinja). This is expensive, as it requires compiling the lexer & parser or templates. Initialization is sped up by pre-caching these in a separate build step, which significantly improves build time. Note that `run_bindings_tests.py` will compile multiple test files in a single Python process_,_ which is quite fast, since initialization is done only once.

The compile has not been profiled (except by using [time(1)](http://en.wikipedia.org/wiki/Time_(Unix)) on a build), since the low-hanging fruit were obvious (cache lexer and parser tables and templates), performance is now good enough, and significant additional improvements are unlikely without major work or reengineering.<

Processing time itself is presumably dominated by the main libraries (parsing via PLY and template rendering via Jinja), and possibly object construction (IdlDefinitions); the actual CG is likely quite cheap, as it's a simple iteration over the IR with a few conditionals and simple computations.

To our knowledge, there are no O(_n_<sup>2</sup>) or slower algorithms in the compiler – everything should be O(_n_) or faster – but some may be lurking somewhere.

### Further improvements
Potential future improvements (without substantial reengineering) would primarily be to improve startup of some components, or rewrite some components in C.

*   Python tweaks:
*   execute bytecode directly (compile to `.pyc` or `.pyo` in separate step and execute that): v. small improvement (about .5 ms/process), would require other build step
*   use Python optimized mode (`-O` or `-OO`): likely little improvement (just shrinks size of bytecode), and currently causes serious regressions (likely due to breaking caching in a library)
*   Further optimization of Jinja startup requires changes in the Jinja library itself; see discussion at references.
*   Replacing parts of the PLY lexer-parser with a C/C++ library (produced by lex or yacc) would speed up execution of the front end, as it would be C code, not Python code, and make it virtually instantaneous. C lex/C++ yacc code is about 50x faster than PLY, and the lexer and parser typically each account for 40% of PLY execution time; remainder is cost of running the yacc actions, in this case to build an AST ([Writing Parsers and Compilers with PLY](http://www.dabeaz.com/ply/PLYTalk.pdf), slides 76 and 77).
*   A C lexer would be simple and significant, while a C++ parser would be relatively complicated, particularly due to the use of inheritance to derive a parser for the Blink IDL dialect from a parser for the standard Web IDL language.
*   <div>In terms of the overall build system, initialization costs could be significantly reduced by initializing a single process and then forking it, so it's just a copy. However, this is platform dependent (Windows does not have `fork()`), and would significantly complicate the compiler.</div>

### References
*   PLY: [Lex: Optimized mode](http://www.dabeaz.com/ply/ply.html#ply_nn15) and [Using Python's Optimized Mode](http://www.dabeaz.com/ply/ply.html#ply_nn38): we use optimization for the control over caching, not because we are running Python in optimized mode (we aren't)
*   [Jinja performance](http://www.chromium.org/developers/jinja#TOC-Performance)

## References
*   [Source/bindings/scripts](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/): the code
*   [Issue <strike>239771</strike>: Rewrite the IDL compiler in Python](https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/s1xTE428OAs): tracking bug for rewrite (2013/2014)
*   [IDL compiler (V8 bindings generator) switched to Python](https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/s1xTE428OAs): wrap-up email for rewrite (Feb 2014)
*   [followup](https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/5FSUhiugWsE) about front end
