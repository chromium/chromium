# Mojo Typescript/JavaScript Bindings

Generated Mojo Typescript bindings are compiled to plain JavaScript bindings for
plain JavaScript clients and to Closure-annotated JavaScript for Closure
clients.

**These bindings are work in progress and are not ready for general use.**

## Getting started

To use the Typescript bindings to generate JS bindings, set
`use_typescript_sources` to true in your mojom target:

```
mojom("interfaces") {
  sources = [
    "echo.mojom",
  ]
  use_typescript_sources = true
}
```

and add `enable_typescript_bindings = true` to your `args.gn`.

## Tests
There are two sets of tests: compilation and runtime.

* Compilation tests are done by including the
`//mojo/public/js/ts/bindings/tests` gn target, which triggers the Typescript
bindings generation and their compilation. This target is included as part of
`mojo_unittests`.

* Runtime tests are done as part of Blink's Web Tests. They are in
`//third_party/blink/web_tests/mojo/bindings-lite-*.html`.
