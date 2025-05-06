# Don't write a clang plugin

[TOC]

Make sure you really want to write a clang plugin.

*   The clang plugin api is not stable. If you write a plugin, _you_ are
    responsible for making sure it's updated when we update clang.
*   If you're adding a generally useful warning, it should be added to upstream
    clang, not to a plugin.
*   You should not use a clang plugin to do things that can be done in a
    PRESUBMIT check (e.g. checking that the headers in a file are sorted).

Valid reasons for writing a plugin are for example:

*   You want to add a chromium-specific error message.
*   You want to write an automatic code rewriter.

In both cases, please inform
[clang@chromium.org](https://groups.google.com/a/chromium.org/group/clang/topics)
of your plans before you pursue them.

# Having said that

clang currently has minimal documentation on its plugin interface; it's mostly
doxygen annotations in the source. This is an attempt to be half map to the
header files/half tutorial.

# Building your plugin

## Just copy the clang build system

I suggest you make a new dir in `llvm-project/clang/examples/` and copy the
Makefile from `PrintFunctionNames` there. This way, you'll just leverage the
existing clang build system. You can then build your plugin with

    make -C llvm-project/clang/examples/myplugin

See [Using plugins](clang.md) on how to use your plugin while building chromium
with clang.

## Use the interface in tools/clang/plugins/ChromeClassTester.h

Here's a canned interface that filters code, only passing class definitions in
non-blacklisted headers. The users of `ChromeClassTester` are good code to study
to see what you can do.

## Or if you're doing something really different, copy PrintFunctionNames.cpp

`PrintFunctionNames.cpp` is a plugin in the clang distribution. It is the Hello
World of plugins. As a most basic skeleton, it's a good starting point. Change
all the identifiers that start with `PrintFunction` to your desired name. Take
note of the final line:

```cpp
static FrontendPluginRegistry::Add<PrintFunctionNamesAction>
X("print-fns", "print function names");
```

This registers your `PluginASTAction` with a string plugin name that can be
invoked on the command line. Note that everything else is in an anonymous
namespace; all other symbols aren't exported.

Your `PluginASTAction` subclass exists just to build your `ASTConsumer`, which
receives declarations, sort of like a SAX parser.

## Your ASTConsumer

There is doxygen documentation on when each `ASTConsumer::Handle` method is
called in `llvm-project/clang/include/clang/AST/ASTConsumer.h`. For this
tutorial, I'll assume you only want to look at type definitions (struct, class,
enum definitions), so we'll start with:

```cpp
class TagConsumer : public ASTConsumer {
  public:
    virtual void HandleTagDeclDefinition(TagDecl *D) {
    }
};
```

The data type passed in is the `Decl`, which is a giant class hierarchy spanning
the following files:

*   `llvm-project/clang/include/clang/AST/DeclBase.h`: declares the `Decl`
    class, along with some utility classes you won't use.
*   `llvm-project/clang/include/clang/AST/Decl.h`: declares subclasses of
    `Decl`, for example, `FunctionDecl` (a function declaration), `TagDecl` (the
    base class for struct/class/enum/etc), `TypedefDecl`, etc.
*   `llvm-project/clang/include/clang/AST/DeclCXX.h`: C++ specific types.
    You'll find most Decl subclasses dealing with templates here,
    along with things like `UsingDirectiveDecl`, `CXXConstructorDecl`, etc.

The interface on these classes is massive; We'll only cover some of the basics,
but some basics about source location and errors.

## Emitting Errors

Lots of location information is stored in the `Decl` tree. Most `Decl`
subclasses have multiple methods that return a `SourceLocation`, but lets use
`TagDecl::getInnerLocStart()` as an example. (`SourceLocation` is defined in
`llvm-project/clang/include/clang/Basic/SourceLocation.h`, for reference.)

Errors are emitted to the user through the `CompilerInstance`. You will probably
want to pass the `CompilerInstance` object passed to
`ASTAction::CreateASTConsumer` to your ASTConsumer subclass for reporting. You
interact with the user through the `Diagnostic` object. You could report errors
to the user like this:

```cpp
void emitWarning(CompilerInstance& instance, SourceLocation loc, const char* error) {
    FullSourceLoc full(loc, instance.getSourceManager());
    unsigned id = instance.getDiagnostics().getCustomDiagID(DiagnosticsEngine::Warning, error);
    DiagnosticBuilder B = instance.getDiagnostics().Report(full, id);
}
```

(The above is the simplest error reporting. See
`llvm-project/clang/include/clang/Basic/Diagnostic.h` for all the things you can
do, like `FixItHint`s if you want to get fancy!)

## Downcast early, Downcast often

The clang library will give you the most general types possible. For example
`TagDecl` has comparably minimal interface. The library is designed so you will
be downcasting all the time, and you won't use the standard `dynamic_cast<>()`
builtin to do it. Instead, you'll use llvm-project/clang's home built RTTI
system:

```cpp
  virtual void HandleTagDeclDefinition(TagDecl* tag) {
    if (CXXRecordDecl* record = dyn_cast<CXXRecordDecl>(tag)) {
      // Do stuff with |record|.
    }
  }
```

## A (not at all exhaustive) list of things you can do with (CXX)RecordDecl

*   Iterate across all constructors (`CXXRecordDecl::ctor_begin()`,
    `CXXReocrdDecl::ctor_end()`)
*   `CXXRecordDecl::isPOD()`: is this a Plain Old Datatype (a type that has no
    construction or destruction semantics)?
*   Check if certain properties of the class: `CXXRecordDecl::isAbstract()`,
    `CXXRecordDecl::hasTrivialConstructor()`,
    `CXXRecordDecl::hasTrivialDestructor()`, etc.
*   Iterate across all fields/member variables (`RecordDecl::field_begin()`,
    `RecordDecl::field_end()`)
*   Iterate across all of the base classes of a record type
    (`CXXRecordDecl::bases_begin()`, `CXXRecordDecl::bases_end()`)
*   Get the simple string name `NamedDecl::getNameAsString()`. (This method is
    deprecated, but the replacement assert()s on error conditions). (If you had
    `struct One {}`, this method would return "One".)

## Modifying existing plugins

If you want to add additional checks to the existing plugins, be sure to add the
new diagnostic behind a flag (there are several examples of this in the plugins
already). The reason for this is that the plugin is bundled with clang, and the
new check will get deployed with the next clang roll. If your check fires, then
the next clang roll would now be blocked on cleaning up the whole codebase for
your check – and even if the check doesn't fire at the moment, maybe that
regresses until the next clang roll happens. If your new check is behind a flag,
then the clang roll can happen first, and you can add the flag to enable your
check after that, and then turn on the check everywhere once you know that the
codebase is clean.
