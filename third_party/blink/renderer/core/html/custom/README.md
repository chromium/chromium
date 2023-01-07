# Custom Elements

Custom elements let authors create their own elements, with their own
methods, behavior, and attribute handling. Custom elements shipped in
M33. We colloquially refer to that version as "v0."

Contact Dominic Cooney
([dominicc@chromium.org](mailto:dominicc@chromium.org)) with
questions.

### Code Location

The custom elements implementation is split between core/dom and
bindings/core/v8.

## Design

### Some Important Classes

###### CustomElementDefinition

The definition of one &lsquo;class&rsquo; of element. This type is
abstract to permit different kinds of definitions, although at the
moment there is only one: ScriptCustomElementDefinition.

ScriptCustomElementDefinition is linked to its constructor by an ID
number. The ID number is stored in a map, keyed by constructor, in a
private property of the CustomElementRegistry wrapper. The ID is an
index into a list of definitions stored in V8PerContextData.

###### CustomElementDescriptor

A tuple of local name, and custom element name. For autonomous custom
elements, these strings are the same; for customized built-in elements
these strings will be different. In that case, the local name is the
element's tag name and the custom element name is related to the value
of the &ldquo;is&rdquo; attribute.

###### CustomElementRegistry

Implements the `window.customElements` property. This maintains the
set of registered names. The wrapper of this object is used by
ScriptCustomElementDefinition to cache state in V8.

###### V8HTMLElement Constructor

The `HTMLElement` interface constructor. When a custom element is
created from JavaScript all we have to go on is the constructor (in
`new.target`); this uses ScriptCustomElementDefinition's state to find
the definition to use.

### Memory Management

Once defined, custom element constructors and prototypes have to be
kept around indefinitely because they could be created in future by
the parser. On the other hand, we must not leak when a window can no
longer run script.

We use a V8PrivateProperty on the CustomElementRegistry wrapper which
points to a map that keeps constructors and prototypes alive. See
ScriptCustomElementDefinition.

## Style Guide

In comments and prose, write custom elements, not Custom Elements, to
match the HTML standard.

Prefix type names with CustomElement (singular).

## Testing

Custom elements have small C++ unit tests and medium
&ldquo;layout&rdquo; tests.

###### C++ Unit Tests

These are in third_party/blink/renderer/core/dom/*_test.cc and are
built as part of the blink_unittests target. The test names start
with CustomElement so you can run them with:

    $ out/Debug/blink_unittests --gtest_filter=CustomElement*

###### Web Tests

The custom element web tests are generally in
third_party/blink/web_tests/custom-elements.

All custom elements web tests use the [web-platform-tests
harness](https://web-platform-tests.org/) and follow its style. The
WPT style is not very prescriptive, so be consistent with other custom
elements tests.

When naming tests, use short names describing what the test is doing.
Avoid articles. For example, "HTMLElement constructor, invoke". When
writing assertion messages, start with a lowercase letter (unless the
word is a proper noun), use normal grammar and articles, and include
the word &ldquo;should&rdquo; to leave no doubt whate the expected
behavior is.

###### Spec Tests

These will be upstreamed to WPT, replacing [the tests for
registerElement](https://github.com/web-platform-tests/wpt/tree/master/custom-elements)
we contributed earlier. To facilitate that, follow these guidelines:

* Keep the tests together in the `spec` directory.
* Only test things required in a spec. The test should permit other
  possible reasonable implementations.
* Avoid using Blink-specific test mechanisms. Don't use
  `window.internals` for example.

###### Implementation Tests

These are for testing Blink-specific details like object lifetimes,
crash regressions, interaction with registerElement and HTML Imports,
and so on.

These tests can use Blink-specific testing hooks like
`window.internals` and `testRunner`.

###### Web Exposed Tests

Finally there are /TODO(dominicc): should be/ tests in
webexposed/custom-elements which assert that we HAVE NOT shipped
`window.customElements.define` yet. These tests need to be updated
when we ship `window.customElements.define` or remove
`registerElement`.

## &ldquo;V0&rdquo; Deprecation

The plan is to:

1. Implement the &lsquo;new&rsquo; kind of custom elements separately
   from the existing implementation. We have renamed all of old
   implementation so that the type names start with V0; it should be
   easy to tell whether you are looking at the old or new
   implementation.
1. When we ship `window.customElements.define`, add a deprecation
   warning to `document.registerElement` directing authors to use the
   new API.
1. Change the &lsquo;web&rsquo; API to use the new kind of custom
   elements. That API is used by extensions to implement the webview
   tag and things like that.
1. When [the use counter for
   registerElement](https://www.chromestatus.com/metrics/feature/timeline/popularity/457)
   drops below 0.03% of page loads, remove the old implementation. We
   may remove it even sooner, if we have evidence that sites are using
   feature detection correctly.

## References

These have links to the parts of the DOM and HTML specs which define
custom elements:

* [WHATWG DOM Wiki: Custom Elements](https://github.com/whatwg/dom/wiki#custom-elements)
* [WHATWG HTML Wiki: Custom Elements](https://github.com/whatwg/html/wiki#custom-elements)
