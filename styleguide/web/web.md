# Chromium Web Development Style Guide

[TOC]

## Where does this style guide apply?

This style guide targets Chromium frontend features implemented with JavaScript,
CSS, and HTML.  Developers of these features should adhere to the following
rules where possible, just like those using C++ conform to the [Chromium C++
styleguide](../c++/c++.md).

This guide follows and builds on:

* [Google HTML/CSS Style Guide](https://google.github.io/styleguide/htmlcssguide.html)
* [Google JavaScript Style Guide](https://google.github.io/styleguide/jsguide.html)
* [Google Polymer Style Guide](http://go/polymer-style)

*** aside
Note: Concerns for browser compatibility are usually not relevant for
Chromium-only code.
***

## Separation of presentation and content

When designing a feature with web technologies, separate the:

* **content** you are presenting to the user (**HTML**)
* **styling** of the data (**CSS**)
* **logic** that controls the dynamic behavior of the content and presentation
  (**JS**)

This highlights the concern of each part of the code and promotes looser
coupling (which makes refactor easier down the road).

Another way to envision this principle is using the MVC pattern:

| MVC Component | Web Component |
|:-------------:|:-------------:|
| Model         | HTML          |
| View          | CSS           |
| Controller    | JS            |

It's also often appropriate to separate each implementation into separate files.

DO:
```html
<!-- missile-button.html -->
<link rel="stylesheet" href="warnings.css">
<b class="warning">LAUNCH BUTTON WARNING</b>
<script src="missile-button.js">
```
```css
/* warnings.css */
.warning {
  color: red;
}
```
```js
// missile-button.js
document.querySelector('b').onclick = fireZeeMissiles;
```

DON'T:
```html
<!-- missile-button.html -->
<b style="color: red;" onclick="fireZeeMissiles()">LAUNCH BUTTON WARNING</b>
```

*** aside
Note: For various technical and historical reasons, code using the Polymer
library may use `on-event`-style event listener wiring and `<style>` tags that
live inside of .html files.
***

## HTML

See the [Google HTML/CSS Style
guide](https://google.github.io/styleguide/htmlcssguide.html).

### Head

```html
<!doctype html>
<html dir="$i18n{direction}">
<head>
  <meta charset="utf-8">
  <title>$i18n{myFeatureTitle}</title>
  <link rel="icon" href="feature.png">
  <link rel="stylesheet" href="feature.css">
  <script src="feature.js"></script>
</head>
…
</html>
```

* Specify `<!doctype html>`.

* Set the `dir` attribute of the html element to the localized ‘textdirection’
  value.  This flips the page visually for RTL languages and allows
  `html[dir=rtl]` selectors to work.

* Specify the charset, UTF-8.

* Link in image, icon and stylesheet resources.
    * Do not style elements with `style="..."` attributes.

* Include the appropriate JS scripts.
    * Do not add JS to element event handlers.

*** aside
Note: Polymer event handlers like `on-click` are allowed and often reduce the
amount of addressing (adding an ID just to wire up event handling).
***

### Body

```html
<h3>$i18n{autofillAddresses}</h3>
<div class="settings-list">
  <list id="address-list"></list>
  <div>
    <button id="autofill-add-address">$i18n{autofillAddAddress}</button>
  </div>
</div>
<if expr="chromeos">
  <a href="https://www.google.com/support/chromeos/bin/answer.py?answer=142893"
      target="_blank">$i18n{learnMore}</a>
</if>
```

* Element IDs use `dash-form`
    * Exception: `camelCase` is allowed in Polymer code for easier
      `this.$.idName` access.

* Localize all strings using $i18n{}

* Use camelCase for $i18n{} keys names.

* Add 2 space indentation in each new block.

* Adhere to the 80-column limit.
    * Indent 4 spaces when wrapping a previous line.

* Use double-quotes instead of single-quotes for all attributes.

* Don't close single tags
    * DO: `<input type="radio">`
    * DON'T: `<input type="radio" />`

*** aside
Note: All `<custom-elements>` and some HTML elements like `<iframe>` require
closing.
***

* Use the `button` element instead of `<input type="button">`.

* Do not use `<br>`; place blocking elements (`<div>`) as appropriate.

* Do not use spacing-only divs; set the margins on the surrounding elements.

* Only use `<table>` elements when displaying tabular data.

* Do not use the `for` attribute of `<label>`
    * If you're labelling a checkbox, put the `<input>` inside the `<label>`
    * If you're labelling purely for accessibility, e.g. a `<select>`, use
      `aria-labelledby`

## CSS

See the [Google HTML/CSS style
guide](https://google.github.io/styleguide/htmlcssguide.html) (and again, browser
compatibility issues are less relevant for Chrome-only code).

```css
.raw-button,
.raw-button:hover,
.raw-button:active {
  --sky-color: blue;
  -webkit-margin-collapse: discard;
  background-color: rgb(253, 123, 42);
  background-repeat: no-repeat;
  border: none;
  min-width: 0;
  padding: 1px 6px;
}
```

* Specify one selector per line.
     * Exception: One rule / one line frames in a `@keyframe` (see below).

* Opening brace on the same line as the last (or only) selector.

* Two-space indentation for each declaration, one declaration per line,
  terminated by a semicolon.

* Use shorthand notation when possible.

* Alphabetize properties.
    * `-webkit` properties should be listed at the top, sorted alphabetically.
    * `--variables` should be alphabetically declared when possible.

* Insert a space after the colon separating property and value.

* Do not create a class for only one element; use the element ID instead.

* When specifying length values, do not specify units for a zero value, e.g.,
  `left: 0px;` becomes `left: 0;`
    * Exception: 0% values in lists of percentages like `hsl(5, 0%, 90%)` or
      within @keyframe directives, e.g:
```css
@keyframe animation-name {
  0% { /* beginning of animation */ }
  100% { /* end of animation */ }
}
```

* Use single quotes instead of double quotes for all strings.

* Don't use quotes around `url()`s unless needed (i.e. a `data:` URI).

* Class names use `dash-form`.

* If time lengths are less than 1 second, use millisecond granularity.
    * DO: `transition: height 200ms;`
    * DON'T: `transition: height 0.2s;`

* Use two colons when addressing a pseudo-element (i.e. `::after`, `::before`,
  `::-webkit-scrollbar`).

* Use scalable `font-size` units like `%` or `em` to respect users' default font
  size

* Don't use CSS Mixins (`--mixin: {}` or `@apply --mixin;`).
    * CSS Mixins are defunct after Oct 2022, see
      [crrev.com/c/3953559](https://crrev.com/c/3953559)
    * All CSS Mixins usages have been removed from Chromium, see
      [crbug.com/973674](https://crbug.com/973674) and
      [crbug.com/1320797](https://crbug.com/1320797)
    * Mixins were [dropped from CSS](https://www.xanthir.com/b4o00) in favor of
      [CSS Shadow Parts](https://drafts.csswg.org/css-shadow-parts/).
    * Instead, replace CSS mixin usage with one of these natively supported
      alternatives:
        * CSS Shadow Parts or CSS variables for styling of DOM nodes residing in
          the Shadow DOM of a child node.
        * Plain CSS classes, for grouping a set of styles together for easy
          reuse.

### Color

* When possible, use named colors (i.e. `white`, `black`) to enhance
  readability.

* Prefer `rgb()` or `rgba()` with decimal values instead of hex notation
  (`#rrggbb`).
    * Exception: shades of gray (i.e. `#333`)

* If the hex value is `#rrggbb`, use the shorthand notation `#rgb`.

### URLs

* Don't embed data URIs in source files. Instead, use a relative path to an icon
  in your UI (and include this icon in the generated grd file), or use an
  absolute URL for an icon from the shared resources at ui/webui/resources:

```css
background-image: url(chrome://resources/images/path/to/image.svg);
```

### RTL

```css
.suboption {
  margin-inline-start: 16px;
}

#save-button {
  color: #fff;
  left: 10px;
}

html[dir='rtl'] #save-button {
  right: 10px;
}
```

Use RTL-friendly versions of things like `margin` or `padding` where possible:

* `margin-left` -> `margin-inline-start`
* `padding-right` -> `padding-inline-end`
* `text-align: left` -> `text-align: start`
* `text-align: right` -> `text-align: end`
* set both `left` for `[dir='ltr']` and `right` for `[dir='rtl']`

For properties that don't have an RTL-friendly alternatives, use
`html[dir='rtl']` as a prefix in your selectors.

## JavaScript/TypeScript

New WebUI code (except for ChromeOS specific code) should be written in
TypeScript.

### Style

See the [Google JavaScript Style
Guide](https://google.github.io/styleguide/jsguide.html) as well as
[ECMAScript Features in Chromium](es.md).

* Use `$('element-id')` instead of `document.getElementById`. This function can
  be imported from util.m.js.

* Use single-quotes instead of double-quotes for all strings.
    * `clang-format` now handles this automatically.

* Use ES5 getters and setters
    * Use `@type` (instead of `@return` or `@param`) for JSDoc annotations on
      getters/setters

* For legacy code using closure, see [Annotating JavaScript for the Closure
  Compiler](https://developers.google.com/closure/compiler/docs/js-for-compiler)
  for @ directives

* Prefer `event.preventDefault()` to `return false` from event handlers

* Prefer `this.addEventListener('foo-changed', this.onFooChanged_.bind(this));`
  instead of always using an arrow function wrapper, when it makes the code less
  verbose without compromising type safety (for example in TypeScript files).

* When using `?.` be aware that information about the original location of the
  null/undefined value can be lost. You should avoid cases like this and instead
  prefer explicit error checking:
```js
const enterKey = keyboards.getCurrentKeyboard()?.getKeys()?.getEnterKey();
// ... Lots of code here.
if (!enterKey) {
  // Something has gone wrong here, but it is unclear what.
}
```

* Don't use `?.` as a way to silence TypeScript "object is possibly null"
  errors. Instead use `assert()` statements. Only use the optional chaining
  feature when the code needs to handle null/undefined gracefully.


### Closure compiler (ChromeOS Ash code only)

* Closure compiler can only be used on ChromeOS Ash. All other platforms
  are required to use TypeScript to add type checking.

* Use the [closure
  compiler](https://chromium.googlesource.com/chromium/src/+/main/docs/closure_compilation.md)
  to identify JS type errors and enforce correct JSDoc annotations.

* Add a `BUILD.gn` file to any new web UI code directory.

* Ensure that your `BUILD.gn` file is included in
  `src/BUILD.gn:webui_closure_compile` (or somewhere in its
  deps hierarchy) so that your code is typechecked in an automated way.

* Type Polymer elements by appending 'Element' to the element name, e.g.
  `/** @type {IronIconElement} */`

* Use explicit nullability in JSDoc type information
    * Rather than `@type {Object}` use:
        * `{!Object}` for only Object
        * `{!Object|undefined}` for an Object that may be undefined
        * `{?Object}` for Object that may be null
    * Do the same for typedefs and Array (or any other nullable type)

* Don't add a `.` after template types
    * DO: `Array<number>`
    * DON'T: `Array.<number>`

* Don't specify string in template object types. That's the only type of key
  `Object` can possibly have.
    * DO: `Object<T>`
    * DON'T: `Object<string, T>`

* Use template types for any class that supports them, for example:
    * `Array`
    * `CustomEvent`
    * `Map`
    * `Promise`
    * `Set`


## Polymer

Also see the [Google Polymer Style Guide](http://go/polymer-style).

* Elements with UI should have their HTML in a .html file and logic in a TS file
  with the same name. The HTML template can be imported into the final JS file
  at runtime from a generated JS wrapper file via the getTemplate() function.
  The wrapper file is generated using the html_to_wrapper gn rule:
```
  html_to_wrapper("html_wrapper_files") {
    in_files = [ "my_app.html" ]
  }
```

* In new code, use class based syntax for custom elements. Example:
```js
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './my_app.html.js';

class MyAppElement extends PolymerElement {
  static get is() {
    return 'my-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      foo: String,
    };
  }

  foo: string;
}

customElements.define(MyAppElement.is, MyAppElement);
```

* Use a consistent ordering for common methods (or, in legacy code, the
  parameters passed to Polymer()):
    * `is`
    * `behaviors` (legacy code only)
    * `properties` (public, then private)
    * `hostAttributes`
    * `listeners`, `observers`
    * `created`, `ready`, `attached`, `detached`
    * public methods
    * event handlers, computed functions, and private methods

* Use camelCase for element IDs to simplify local DOM accessors (i.e.
  `this.$.camelCase` instead of `this.$['dash-case']`).

* Note: In TypeScript, the `this.$.camelCase` accessor requires adding an
  interface:

```js
interface MyAppElement {
  $: {
    camelCase: HTMLElement,
  };
}
```

* Use `this.foo` instead of `newFoo` arguments in observers when possible.
  This makes changing the type of `this.foo` easier (as the `@type` is
  duplicated in less places, i.e. `@param`).

```js
static get properties() {
  return {
    foo: {type: Number, observer: 'fooChanged_'},
  };
}

/** @private */
fooChanged_() {
  this.bar = this.derive(this.foo);
}
```

* Use native `on-click` for click events instead of `on-tap`. 'tap' is a
  synthetic event provided by Polymer for backward compatibility with some
  browsers and is not needed by Chrome.

* Make good use of the  [`dom-if` template](
https://www.polymer-project.org/2.0/docs/devguide/templates#dom-if):
  * Consider using `dom-if` to lazily render parts of the DOM that are hidden by
  default. Also consider using [`cr-lazy-render`](
  https://cs.chromium.org/chromium/src/ui/webui/resources/cr_elements/cr_lazy_render/cr_lazy_render.js)
  instead.
  * **Only use `dom-if`** if the DOM subtree is non-trivial, defined as:
      * Contains more than 10 native elements, OR
      * Contain **any** custom elements, OR
      * Has many data bindings, OR
      * Includes non-text content (e.g images).

    For trivial DOM subtrees using the HTML [`hidden` attribute](
    https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/hidden)
    yields better performance, than adding a custom `dom-if` element.

* Do not add iron-icons dependency to third_party/polymer/.
  * Polymer provides icons via the `iron-icons` library, but importing each of the iconsets means importing hundreds of SVGs, which is unnecessary because Chrome uses only a small subset.
  * Alternatives:
    * Include the SVG in a WebUI page-specific icon file. e.g. `chrome/browser/resources/settings/icons.html`.
    * If reused across multiple WebUI pages, include the SVG in `ui/webui/resources/cr_elements/icons.html` .
  * You may copy the SVG code from [iron-icons files](https://github.com/PolymerElements/iron-icons/blob/master/iron-icons.js).

## Grit processing

Grit is a tool that runs at compile time to pack resources together into
Chromium. Resources are packed from grd files. Most Chromium WebUI resources
should be located in autogenerated grd files created by the [`generate_grd`](
https://chromium.googlesource.com/chromium/src/+/main/docs/webui_build_configuration.md#generate_grd)
gn rule.

### Preprocessing

Sometimes it is helpful to selectively include or exclude code at compile-time.
This is done using the [`preprocess_if_expr`][preprocess_if_expr_doc] gn rule,
which processes files for `<if expr>` without running the entire grit resource
packing process.

`<if expr>` tags allow conditional logic by evaluating an expression of grit
variables in a compile-time environment.

The grit variables are provided to grit through the `defines` argument of
`preprocess_if_expr` ([sample search][defines_search]). For some widely
available variables, see [//tools/grit/grit_args.gni][grit_args] and
[//chrome/common/features.gni][chrome_features].

These allow conditionally including or excluding code. For example:

```ts
function isWindows(): boolean {
  // <if expr="is_win">
  return true;
  // </if>
  // <if expr="not is_win">
  return false;
  // </if>
}
```

***aside
Note: Preprocessor statements can live in places that surprise linters or
formatters (for example: a .ts file with an `<if>` in it will make PRESUBMIT
ESLint checks fail). Putting these language-invalid features inside of comments
helps alleviate problems with unexpected input.
***

[preprocess_if_expr_doc]: https://chromium.googlesource.com/chromium/src/+/main/docs/webui_build_configuration.md#preprocess_if_expr
[defines_search]: https://source.chromium.org/search?q=preprocess_if_expr%20defines&ss=chromium
[grit_args]: https://crsrc.org/c/tools/grit/grit_args.gni?q=_grit_defines
[chrome_features]: https://crsrc.org/c/chrome/common/features.gni?q=chrome_grit_defines

#### Example

The following BUILD.gn example code uses `preprocess_if_expr` to preprocess any
`<if expr>` in my_app.ts and in the my_app.html, exposing gn variables to Grit.
It then wraps the html file (see the earlier `html_to_wrapper` example), runs
the TypeScript compiler on the outputs of this operation and uses the manifest
from this operation and the `in_files` option to place both the final,
preprocessed file and a separate (not preprocessed) icon into a generated grd
file using `generate_grd`:

```
preprocess_folder = "preprocessed"
preprocess_manifest = "preprocessed_manifest.json"

preprocess_if_expr("preprocess") {
  defines = ["is_win=$is_win"]
  in_folder = "."
  in_files = [ "my_app.ts", "my_app.html" ]
  out_folder = "$target_gen_dir/$preprocess_folder"
}

html_to_wrapper("html_wrapper_files") {
  in_folder = "$target_gen_dir/$preprocess_folder"
  in_files = [ "my_app.html" ]
  out_folder = "$target_gen_dir/$preprocess_folder"
  deps = [":preprocess"]
}

# Run TS compiler on the two files:
ts_library("build_ts") {
  root_dir = "$target_gen_dir/$preprocess_folder"
  out_dir = "$target_gen_dir/tsc"
  tsconfig_base = "tsconfig_base.json"
  in_files = [
    "my_app.html.ts",
    "my_app.ts",
  ]
  deps = [
    "//third_party/polymer/v3_0:library",
    "//ui/webui/resources/js:build_ts",
  ]
  extra_deps = [
    ":preprocess",
    ":html_wrapper_files",
  ]
}

# Put the compiled files as well as a separate my_icon.svg file in the grd:
generate_grd("build_grd") {
  input_files = [ "my_icon.svg" ]
  input_files_base_dir = rebase_path(".", "//")
  deps = [ ":build_ts" ]
  manifest_files = [ "$target_gen_dir/tsconfig.manifest" ]
  grd_prefix = [ "foo" ]
  out_grd = "$target_gen_dir/resources.grd"
}
```

*** aside
Note:
In a few legacy resources, preprocessing is enabled by adding the
`preprocess="true"` attribute inside of a `.grd` file on `<structure>` and
`<include>` nodes.
***

### Inlining resources with Grit (deprecated, don't use)

`<include src="[path]">` reads the file at `path` and replaces the `<include>`
tag with the file contents of `[path]`.

Don't use `<include>` in new JS code;
[it is being removed](https://docs.google.com/document/d/1Z18WTNv28z5FW3smNEm_GtsfVD2IL-CmmAikwjw3ryo/edit?usp=sharing#heading=h.66ycuu6hfi9n).
Instead, use JS imports. If there is concern about importing a large number of
JS files, the optimize_webui build rule supports bundling pages using Rollup.

Some legacy UIs use Grit to read and inline resources via `flattenhtml="true"`.
This option should not be used in new code; instead, use JS imports and bundling
as needed. Icons can also be placed in an iconset, to avoid importing them
individually.

*** aside
Note: The implementation of flattening does HTML parsing and URL detection via
regular expressions and is not guaranteed to work in all cases. In particular,
it does not work with any generated resources.
***
