<!-- Feature template markdown:
## Header

**Usage Example:**

```js

```

**Documentation:** [link]()

**Discussion Notes / Link to Thread:**

hyphen-hyphen-hyphen (change to actual hyphen)
-->

<style>
  .doc {
    font-size: 16px;
  }

  .doc h2[id] {
    line-height: 20px;
    font-size: 16px;
  }

  .doc h2 > code {
    font-size: 16px;
    font-weight: bold;
  }

  .feature-container {
    background-color: #e8eef7;
    border: 1px solid #c3d9ff;
    margin-bottom: 5px;
    border-radius: 5px;
  }

  .feature-container > h2 {
    cursor: pointer;
    background-color: #c3d9ff;
    margin: 0;
    padding: 5px;
    border-radius: 5px;
  }

  .feature-container > *:not(h2){
    display: none;
    padding: 0px 10px;
  }

  .feature-container.open > *:not(h2){
    display: block;
  }
</style>

<script>
document.addEventListener('DOMContentLoaded', function(event) {
  // Move all headers and corresponding contents to an accordion container.
  document.querySelectorAll('h2[id]').forEach(function(header) {
    const container = document.createElement('div');
    container.classList.add('feature-container');
    header.parentNode.insertBefore(container, header);

    // Add all the following siblings until it hits an <hr>.
    let el = header;
    while (el && el.tagName !== 'HR') {
      var nextEl = el.nextElementSibling;
      container.append(el);
      el = nextEl;
    }

    // Add handler to open accordion on click.
    header.addEventListener('click', function() {
      header.parentNode.classList.toggle('open');
    });
  });

  // Then remove all <hr>s since everything's accordionized.
  document.querySelectorAll('hr').forEach(function(el) {
    el.parentNode.removeChild(el);
  });
});
</script>

[TOC]

# ES2015 Support In Chromium

This is a list of [ECMAScript 6 a.k.a.
ES2015](https://developer.mozilla.org/en-US/docs/Web/JavaScript/New_in_JavaScript/ECMAScript_6_support_in_Mozilla)
features allowed in Chromium code.

This is **not** a status list of [V8](https://developers.google.com/v8/)'s
support for language features.

> **TBD:** Do we need to differentiate per-project?

> **TBD:** Cross-platform build support? As in: transpilers?

You can propose changing the status of a feature by sending an email to
chromium-dev@chromium.org. Include a short blurb on what the feature is and why
you think it should or should not be allowed, along with links to any relevant
previous discussion. If the list arrives at some consensus, send a codereview
to change this file accordingly, linking to your discussion thread.

> Some descriptions and usage examples were taken from
> [kangax](https://kangax.github.io/compat-table/es6/)
> and [http://es6-features.org/](http://es6-features.org/)

# Allowed Features

The following features are allowed in Chromium development.

## => (Arrow Functions)

Arrow functions provide a concise syntax to create a function, and fix a number
of difficulties with `this` (e.g. eliminating the need to write `const self =
this`). Particularly useful for nested functions or callbacks.

Prefer arrow functions over `.bind(this)`.

Arrow functions have an implicit return when used without a body block.

**Usage Example:**

```js
// General usage, eliminating need for .bind(this).
setTimeout(() => {
  this.doSomething();
}, 1000);  // no need for .bind(this) or const self = this.

// Another example...
window.addEventListener('scroll', (event) => {
  this.doSomething(event);
});  // no need for .bind(this) or const self = this.

// Implicit return: returns the value if expression not inside a body block.
() => 1  // returns 1.
() => {1}  // returns undefined - body block does not implicitly return.
() => {return 1;}  // returns 1.
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-arrow-function-definitions)

**Discussion Notes / Link to Thread:**

**Note**: => does not work in iOS9.  Don't use it in code that runs on Chrome for
iOS.  There's a presubmit check that should warn you about this.

[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/iJrC4PVSfoU)

---

## Promise

The Promise object is used for asynchronous computations. A Promise represents a
value which may be available now, or in the future, or never.

**Usage Example:**

```js
/** @type {!Promise} */
const fullyLoaded = new Promise(function(resolve) {
  function isLoaded() { return document.readyState == 'complete'; }

  if (isLoaded()) {
    resolve();
  } else {
    document.onreadystatechange = function() {
      if (isLoaded()) resolve();
    };
  }
});

// ... some time later ...
fullyLoaded.then(startTheApp).then(maybeShowFirstRun);
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-promise-objects)
[link](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)

**Discussion Notes:** Feature already extensively used prior to creation of
this document.

---

## Classes

OOP-style and boilerplate-free class syntax, including inheritance, `super()`,
static members, and getters and setters.

**Usage Example:**

```js
class Shape {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}
// Note: const Shape = class {...}; is also valid.

class Rectangle extends Shape {
  constructor(x, y, width, height) {
    super(id, x, y);
    this.width  = width;
    this.height = height;
  }

  static goldenRectangle() {
    const PHI = (1 + Math.sqrt(5)) / 2;
    return new Rectangle(0, 0, PHI, 1);
  }
}
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-class-definitions)

**Discussion Notes / Link to Thread:**
https://groups.google.com/a/chromium.org/d/msg/chromium-dev/S1h-0m2ohOw/jyaiMGDlCwAJ

**Note**: Not fully supported in iOS9.  Don't use it in code that runs on Chrome
for iOS, unless you can verify it works. TODO: Remove this note once support for
iOS9 is dropped.

---

## Map

A simple key/value map in which any value (both objects and primitive values)
may be used as either a key or a value.

**Usage Example:**

```js
const map = new Map();
map.size === 0;  // true
map.get('foo');  // undefined

const key = 54;
map.set(key, 123);
map.size === 1;  // true
map.has(key);  // true
map.get(key);  // 123

map.delete(key);
map.has(key);  // false
map.size === 0;  // true
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-map-objects)
[link](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map)

**Discussion Notes:** Feature already extensively used prior to creation of
this document.

---

## Set

An object that lets you store unique values of any type, whether primitive
values or object references.

**Usage Example:**

```js
const set = new Set();

set.add(123);
set.size();  // 1
set.has(123);  // true

set.add(123);
set.size();  // 1
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-set-objects)
[link](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set)

**Discussion Notes:** Feature already extensively used prior to creation of
this document.

---

## const (Block-Scoped Constants)

Constants (also known as "immutable variables") are variables which cannot be
re-assigned new content. Note that if the value is an object, the object itself
is still mutable.

`const` is block-scoped, just like `let`.

**Usage Example:**

```js
const gravity = 9.81;
gravity = 0;  // TypeError: Assignment to constant variable.
gravity === 9.81;  // true

const frobber = {isFrobbing: true};
frobber = {isFrobbing: false};  // TypeError: Assignment to constant variable.
frobber.isFrobbing = false;  // Works.
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-let-and-const-declarations)

**See also:** [Object.freeze()](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze)

**Discussion Notes / Link to Thread:** [link](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/MJhTok8Usr8/XCrkisaBBQAJ)

**Note**: `const` [not fully supported](https://caniuse.com/#feat=let) in iOS9.
Don't use it in code that runs on Chrome for iOS, until support for iOS9 is
dropped.

---

## let (Block-Scoped Variables)

`let` declares a variable within the scope of a block, like `const`.
This differs from `var`, which uses function-level scope.

**Usage Example:**

```js
function varTest() {
  var x = 1;
  if (true) {
    var x = 2;  // Same variable!
    console.log(x);  // 2
  }
  console.log(x);  // 2
}

function letTest() {
  let x = 1;
  if (true) {
    let x = 2;  // Different variable.
    console.log(x);  // 2
  }
  console.log(x);  // 1
}

// Redeclaration throws.
function f() {
  var a = 'hello';
  var a = 'hola';  // No error!

  let b = 'world';
  let b = 'mundo;  // TypeError Identifier 'b' has already been declared.
}
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-let-and-const-declarations)

**Discussion Notes / Link to Thread:** [link](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/MJhTok8Usr8/XCrkisaBBQAJ)

**Note**: `let` is [not fully supported](https://caniuse.com/#feat=let) in iOS9.
Don't use it in code that runs on Chrome for iOS, until support for iOS9 is
dropped.

---

## Array Static & Prototype Methods

**Usage Example:**

```js
// Static methods
const a1 = Array.from(document.querySelectorAll('div'));
const a2 = Array.of(7);

// Prototype methods
['a', 'b', 'c', 'd'].copyWithin(2, 0);  // Returns ['a', 'b', 'a', 'b']
[2, 4, 6, 8].find(i => i == 6);  // Returns 6
[2, 4, 6, 8].findIndex(i => i == 6); // Returns 2
[2, 4, 6, 8].fill(1);  // Returns [1, 1, 1, 1]

[2, 4, 6, 8].keys();  // Returns an Array iterator
[2, 4, 6, 8].entries();  // Returns an Array iterator
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-properties-of-the-array-constructor)

**Discussion Notes / Link to Thread:** [link](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/d_2zUYQZJTg/-_PSji_OAQAJ)

---

## Number Properties

**Usage Example:**

```js
// Number.isFinite
// Number.isInteger
// Number.isSafeInteger
// Number.isNaN
// Number.EPSILON
// Number.MIN_SAFE_INTEGER
// Number.MAX_SAFE_INTEGER
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-isfinite-number)

**Discussion Notes / Link to Thread:** [link](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/d_2zUYQZJTg/-_PSji_OAQAJ)

---

## Object Static Methods

**Usage Example:**

```js
// Object.assign
var o = Object.assign({a:true}, {b:true}, {c:true});  // {a: true, b: true, c: true}
'a' in o && 'b' in o && 'c' in o;  // true

// Object.setPrototypeOf
Object.setPrototypeOf({}, Array.prototype) instanceof Array;  // true

// Object.is
Object.is(null, null)  // true
Object.is(NaN, NaN)  // true
Object.is(-0, +0)  // false, btw: -0 === +0 is true

```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-properties-of-the-object-constructor)

**Discussion Notes / Link to Thread:** [link](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/d_2zUYQZJTg/-_PSji_OAQAJ)

---

## for...of Loops

Convenient operator to iterate over all values in an iterable collection. This
differs from `for...in`, which iterates over all enumerable properties of an
object.

**Usage Example:**

```js
// Given an iterable collection of Fibonacci numbers...
for (const n of fibonacci) {
  console.log(n);  // 1, 1, 2, 3, ...
}
```

**Documentation:** [link1](https://tc39.github.io/ecma262/#sec-for-in-and-for-of-statements)
[link2](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/for...of)

**Discussion Notes / Link to Thread:** [link](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/d_2zUYQZJTg/-_PSji_OAQAJ)

---

## Template Literals

Expression interpolation for Strings, with the ability to access raw template
pieces.

**Usage Example:**

```js
// Simple example
const greeting = 'hello';
const myName = {first: 'Foo', last: 'Bar'};
const from = 1900;
const to = 2000;

var message = `${greeting}, I am ${myName.first}${myName.last},
and I am ${to - from} years old`;
// message == 'hello,\nI am FooBar,\nand I am 100 years old'
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-template-literals)

**Discussion Notes / Link to Thread:**

---

# Banned Features

The following features are banned for Chromium development.

# Features To Be Discussed

The following features are currently disallowed. See the top of this page on
how to propose moving a feature from this list into the allowed or banned
sections.

## Block Scope Functions

**Usage Example:**

```js
{
  function foo() {
    return 1;
  }
  // foo() === 1
  {
    function foo() {
      return 2;
    }
    // foo() === 2
  }
  // foo() === 1
}
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-functiondeclarationinstantiation)

**Discussion Notes / Link to Thread:**

---

## Default Function Parameters

Initialize parameters with default values if no value or `undefined` is passed.

**Usage Example:**

```js
/**
 * @param {!Element} element An element to hide.
 * @param {boolean=} animate Whether to animatedly hide |element|.
 */
function hide(element, animate=true) {
  function setHidden() { element.hidden = true; }

  if (animate)
    element.animate({...}).then(setHidden);
  else
    setHidden();
}

hide(document.body);  // Animated, animate=true by default.
hide(document.body, false);  // Not animated.
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-functiondeclarationinstantiation)
[link](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Default_parameters)

**Discussion Notes / Link to Thread:**

---

## Rest Parameters

Aggregation of function arguments into one Array variable.

**Usage Example:**

```js
function usesRestParams(a, b, ...theRest) {
  console.log(a);  // 'a'
  console.log(b);  // 'b'
  console.log(theRest);  // [1, 2, 3]
}

usesRestParams('a', 'b', 1, 2, 3);
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-function-definitions)

**Discussion Notes / Link to Thread:**

---

## Spread Operators

Spreading the elements from an iterable collection into individual literals as
function parameters.

**Usage Example:**

```js
// Spreading an Array
const params = ['hello', true, 7];
const other = [1, 2, ...params];  // [1, 2, 'hello', true, 7]

// Spreading a String
const str = 'foo';
const chars = [...str];  // ['f', 'o', 'o']
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-argument-lists-runtime-semantics-argumentlistevaluation)

**Discussion Notes / Link to Thread:**

---

## Object Literal Extensions

Convenient new ways for object property definition.

**Usage Example:**

```js
// Computed property name
const prop = 'foo';
const o = {
  [prop]: 'hey',
  ['b' + 'ar']: 'there',
};
console.log(o);  // {foo: 'hey', bar: 'there'}

// Shorthand property
const foo = 1;
const bar = 2;
const o = {foo, bar};
console.log(o);  // {foo: 1, bar: 2}

// Method property
const clearSky = {
  // Basically the same as clouds: function() { return 0; }.
  clouds() { return 0; },
  color() { return 'blue'; },
};
console.log(clearSky.color());  // 'blue'
console.log(clearSky.clouds());  // 0
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-object-initialiser)
[link](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Object_initializer)

**Discussion Notes / Link to Thread:**

---

## Binary & Octal Literals

**Usage Example:**

```js
0b111110111 === 503;
0o767 === 503;
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-literals-numeric-literals)

**Discussion Notes / Link to Thread:**

---

## `/u` Unicode Regex Literal

**Usage Example:**

```js
'𠮷'.match(/./u)[0].length === 2;
```

**Documentation:** [link](https://mathiasbynens.be/notes/es6-unicode-regex)

**Discussion Notes / Link to Thread:**

---

## `\u{}` Unicode String

**Usage Example:**

```js
'\u{1d306}' == '\ud834\udf06';  // true
```

**Documentation:** [link](https://mathiasbynens.be/notes/javascript-unicode#escape-sequences)

**Discussion Notes / Link to Thread:**

---

## `/y` Regex Sticky Matching

Keep the matching position sticky between matches and this way support
efficient parsing of arbitrarily long input strings, even with an arbitrary
number of distinct regular expressions.

**Usage Example:**

```js
var re = new RegExp('yy', 'y');
re.lastIndex = 3;
var result = re.exec('xxxyyxx')[0];
result === 'yy' && re.lastIndex === 5;  // true
```

**Documentation:** [link](http://es6-features.org/#RegularExpressionStickyMatching)

**Discussion Notes / Link to Thread:**

---

## Destructuring Assignment

Flexible destructuring of collections or parameters.

**Usage Example:**

```js
// Array
const [a, , b] = [1, 2, 3];  // a = 1, b = 3

// Object
const {width, height} = document.body.getBoundingClientRect();
// width = rect.width, height = rect.height

// Parameters
function f([name, val]) {
  console.log(name, val);  // 'bar', 42
}
f(['bar', 42, 'extra 1', 'extra 2']);  // 'extra 1' and 'extra 2' are ignored.

function g({name: n, val: v}) {
  console.log(n, v);  // 'foo', 7
}
g({name: 'foo', val:  7});

function h({name, val}) {
  console.log(name, val);  // 'bar', 42
}
h({name: 'bar', val: 42});

```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-destructuring-assignment)

**Discussion Notes / Link to Thread:**

---

## Modules

Support for exporting/importing values from/to modules without global
namespace pollution.

**Usage Example:**

```js
// lib/rect.js
export function getArea() {...};
export {width, height, unimportant};

// app.js
import {getArea, width, height} from './lib/rect.js';
```

**Documentation:** [link](https://developers.google.com/web/fundamentals/primers/modules)

**Discussion Notes / Link to Thread:**

---

## Symbol Type

Unique and immutable data type to be used as an identifier for object
properties.

**Usage Example:**

```js
const foo = Symbol();
const bar = Symbol();
typeof foo === 'symbol';  // true
typeof bar === 'symbol';  // true
const obj = {};
obj[foo] = 'foo';
obj[bar] = 'bar';
JSON.stringify(obj);  // {}
Object.keys(obj);  // []
Object.getOwnPropertyNames(obj);  // []
Object.getOwnPropertySymbols(obj);  // [foo, bar]
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-symbol-constructor)

**Discussion Notes / Link to Thread:**

---

## String Static & Prototype methods

**Usage Example:**

```js
// String.raw
// String.fromCodePoint

// String.prototype.codePointAt
// String.prototype.normalize
// String.prototype.repeat
// String.prototype.startsWith
// String.prototype.endsWith
// String.prototype.includes
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-properties-of-the-string-constructor)

**Discussion Notes / Link to Thread:**

---

## Iterators

**Usage Example:**

```js
const fibonacci = {
  [Symbol.iterator]() {
    let pre = 0, cur = 1;
    return {
      next () {
        [pre, cur] = [cur, pre + cur];
        return {done: false, value: cur};
      }
    };
  }
};
```

**Documentation:** [link]()

**Discussion Notes / Link to Thread:**

---

## Generators

Special iterators with specified pausing points.

**Usage Example:**

```js
function* range(start, end, step) {
  while (start < end) {
    yield start;
    start += step;
  }
}

for (const i of range(0, 10, 2)) {
  console.log(i);  // 0, 2, 4, 6, 8
}

```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-generator-function-definitions)

**Discussion Notes / Link to Thread:**

---

## WeakMap

WeakMap does not prevent garbage collection if nothing else refers to an object
within the collection.

**Usage Example:**

```js
const key = {};
const weakmap = new WeakMap();

weakmap.set(key, 123);

weakmap.has(key) && weakmap.get(key) === 123;  // true
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-weakmap-objects)

**Discussion Notes / Link to Thread:**

---

## WeakSet

WeakSet does not prevent garbage collection if nothing else refers to an object
within the collection.

**Usage Example:**

```js
const obj1 = {};
const weakset = new WeakSet();

weakset.add(obj1);
weakset.add(obj1);

weakset.has(obj1);  // true
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-weakset-objects)

**Discussion Notes / Link to Thread:**

---

## Typed Arrays

A lot of new typed Arrays...

**Usage Example:**

```js
new Int8Array();
new UInt8Array();
new UInt8ClampedArray();
// ... You get the idea. Click on the Documentation link below to see all.
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-typedarray-objects)

**Discussion Notes / Link to Thread:**

---

## Proxy

Hooking into runtime-level object meta-operations.

**Usage Example:**

```js
const keyTracker = new Proxy({}, {
  keysCreated: 0,

  get (receiver, key) {
    if (key in receiver) {
      console.log('key already exists');
    } else {
      ++this.keysCreated;
      console.log(this.keysCreated + ' keys created!');
      receiver[key] = true;
    }
  },
});

keyTracker.key1;  // '1 keys created!'
keyTracker.key1;  // 'key already exists'
keyTracker.key2;  // '2 keys created!'
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-proxy-object-internal-methods-and-internal-slots)

**Discussion Notes / Link to Thread:**

---

## Reflection

Make calls corresponding to the object meta-operations.

**Usage Example:**

```js
const obj = {a: 1};
Object.defineProperty(obj, 'b', {value: 2});
obj[Symbol('c')] = 3;
Reflect.ownKeys(obj);  // ['a', 'b', Symbol(c)]
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-reflection)

**Discussion Notes / Link to Thread:**

---

## Math Methods

A lot of new Math methods.

**Usage Example:**

```js
// See Doc
```

**Documentation:** [link](https://tc39.github.io/ecma262/#sec-math)

**Discussion Notes / Link to Thread:**

---
