# Design of V8 bindings

This document explains key concepts in the V8 binding architecture
except the lifetime management of DOM wrappers.
See [V8GCController.md](V8GCController.md) to learn the lifetime management.

[TOC]

## Isolate

An isolate is a concept of an instance in V8.
In Blink, isolates and threads are in 1:1 relationship.
One isolate is associated with the main thread.
One isolate is associated with one worker thread.
An exception is a compositor worker where one isolate is shared by multiple
compositor workers.

## Context

A context is a concept of a global variable scope in V8.
Roughly speaking, one window object corresponds to one context.
For example, `<iframe>` has a window object different from a window object
of its parent frame. So the context of the `<iframe>` is different from
the context of the parent frame. Since these contexts create their own
global variable scopes, global variables and prototype chains of the `<iframe>`
are isolated from the ones of the parent frame.

Here is an example:

```html
// main.html
<html><body>
<iframe src="iframe.html"></iframe>
<script>
var foo = 1234;
String.prototype.substr =
    function (position, length) { // Hijacks String.prototype.substr
        console.log(length);
        return "hijacked";
    };
</script>
</body></html>

// iframe.html
<script>
console.log(foo);  // undefined
var bar = "aaaa".substr(0, 2);  // Nothing is logged.
console.log(bar);  // "aa"
</script>
```

In summary, each frame has a window object.
Each window object has a context.
Each context has its own global variable scope and prototype chains.

## Entered context and current context

A relationship between isolates and contexts is interesting.
One isolate has to execute JavaScripts in multiple frames,
each of which has its own context. This means that the context associated
with the isolate changes over time. In other words, the relationship between
isolates and contexts is 1:N over the lifetime of the isolate.

Here we have a concept of an entered context and a current context.
To understand the difference, you need to understand two kinds of
runtime stacks.

The first stack is a stack of JavaScript functions.
This stack is managed by V8. When one function calls another function,
the callee function is pushed onto the stack. When that function returns,
the function is popped from the stack and the control returns to the caller
function that is now on the top of the stack. Each function has
an associated context. We call the context of the function
that is currently running (i.e., the context of the function that is on the top
of the stack) a current context.

Here is an example:

```html
// main.html
<html><body>
<iframe src="iframe.html"></iframe>
<script>
var iframe = document.querySelector("iframe");
iframe.onload = function () {
    iframe.contentWindow.func();
}
</script>
</body></html>

// iframe.html
<script>
function func() {
  ...;
}
</script>
```

In the above example, at the point when func() is running,
the current context is the context of the `<iframe>`.

There is a second stack that operates on a much coarser granularity.
This stack is managed by V8 binding (not by V8).
When V8 binding invokes JavaScript, V8 binding enters a context
and pushes the context onto the stack.
The JavaScript starts running on the context. When the JavaScript finishes
and the control returns back to V8 binding, V8 binding pops the context
from the stack. Given that the control between V8 binding and V8 can be nested
(i.e., V8 binding invokes JavaScript, which calls into V8 binding,
which invokes another JavaScript etc), these contexts form a stack.
The pushing and popping are done by any V8 API that takes a context argument
or by explicitly calling v8::Context::Enter() and v8::Context::Exit().
We call the most recently entered context an entered context.

In the above example, at the point when func() is running,
the entered context is the context of the main frame
(not the context of `<iframe>`).

The entered context is a concept to implement the
[entry settings object](https://html.spec.whatwg.org/C/#entry-settings-object)
of the HTML spec. The current context is a concept to implement the
[incumbent settings object](https://html.spec.whatwg.org/C/#incumbent-settings-object)
of the HTML spec.

In summary, the entered context is a context from which the current JavaScript
execution was started. The current context is a context of
the JavaScript function that is currently running.

There is another special context called a debugger context.
If a debugger is active, the debugger context may be inserted to
the context stack.

## World

A world is a concept to sandbox DOM wrappers among content scripts of
Chrome extensions. There are three kinds of worlds: a main world,
an isolated world and a worker world.
A main world is a world where a normal JavaScript downloaded from the web
is executed.
An isolated world is a world where a content script of a Chrome extension is
executed.
An isolate of the main thread has 1 main world and N isolated worlds.
An isolate of a worker thread has 1 worker world and 0 isolated world.
[This diagram](https://drive.google.com/file/d/0B1obCOyvTnPKQmJEWkVtOEN2TmM/view?usp=sharing)
will be helpful to understand the relationship.

All worlds in one isolate share underlying C++ DOM objects,
but each world has its own DOM wrappers. That way the worlds in one isolate
can operate on the same C++ DOM object without sharing any DOM wrapper
among the worlds.

Also each world has its own context.
This means that each world has its own global variable scope and
prototype chains.

As a result of the sandboxing, the worlds in one isolate cannot share
any DOM wrappers or contexts but can share underlying C++ DOM objects.
The fact that no DOM wrappers or contexts are shared means that no JavaScript
objects are shared among the worlds. That way we guarantee the security model
that Chrome extensions doesn't share any JavaScript objects while sharing
the underlying C++ DOM objects. This sandbox allows the Chrome extensions to run
untrusted JavaScripts on a shared DOM structure.

(Note: An isolated world is a concept of V8 binding,
whereas an isolate and a context are a concept of V8.
V8 does not know what isolated worlds are in an isolate.)

In summary, an isolate of the main thread consists of 1 main world
and N isolated worlds. An isolate of a worker thread consists of
1 worker world and 0 isolated world. All worlds in one isolate share the
underlying C++ DOM objects, but each world has its own DOM wrappers.
Each world has its own context and thus has its own global variable scope
and prototype chains.

## A relationship between isolates, contexts, worlds and frames

Let's wrap up the relationship between isolates, contexts, worlds and frames.

* As a requirement of the DOM side, one HTML page has N frames.
Each frame has its own context.

* As a requirement of the JavaScript side, one isolate has M worlds.
Each world has its own context.

As a result, when we execute the main thread where N frames and M worlds
are involved, there exists N * M contexts. In other words, one context is
created for each pair of (frame, world).
[This diagram](https://drive.google.com/file/d/0B1obCOyvTnPKSERSMmpRVjVKQWc/view?usp=sharing)
will be helpful to understand the relationship.

The main thread can have only one current context at one time,
but the main thread can have the N * M contexts over its lifetime.
For example, when the main thread is operating on a frame X using a JavaScript
in a world Y, the current context is set to a context for the pair of (X, Y).
The current context of the main thread changes over its lifetime.

On the other hand, a worker thread has 0 frame and 1 world.
Thus a worker thread has only 1 context.
The current context of the worker thread never changes.

## DOM wrappers and worlds

For compatibility reasons, we need to make sure that the same DOM wrapper
is returned to JavaScript as long as the underlying C++ DOM object is alive.
We should not return different DOM wrappers for the same C++ DOM object.

Here is an example:

```html
var div = document.createElement("div");
div.foo = 1234;  // expando
var p = document.createElement("p");
p.appendChild(div);
div = null;
gc();
console.log(p.firstChild.foo);  // This should be 1234, not undefined
```

To accomplish the semantics that the same DOM wrapper is returned to JavaScript
as long as the underlying C++ DOM object is alive, we need a mapping
from the C++ DOM objects to the DOM wrappers.
In addition, we need to sandbox DOM wrappers in each world.
To meet the requirements, we make each world hold a DOM wrapper storage
that stores a mapping from the C++ DOM objects to the DOM wrappers in that world.

As a result, we have multiple DOM wrapper storages in one isolate.
The mapping of the main world is written in `ScriptWrappable`.
If `ScriptWrappable::main_world_wrapper_` has a non-empty value, it is a DOM
wrapper of the C++ DOM object of the main world.
The mapping of other worlds are written in `DOMDataStore`.

## DOM wrappers and contexts

When you create a new DOM wrapper, you need to choose a correct context
on which the DOM wrapper is created. If you create a new DOM wrapper in a
wrong context, you will end up with leaking JavaScript objects to other
contexts, which is very likely to cause security issues.

Here is an example:

```html
// main.html
<html><body>
<iframe src="iframe.html"></iframe>
<script>
var iframe = document.querySelector("iframe");
iframe;  // The wrapper of the iframe should be created in the context of the main frame.
iframe.contentDocument;  // The wrapper of the document should be created in the context of the iframe.
iframe.contentDocument.addEventListener("click",
    function (event) {  // The wrapper of the event should be created in the context of the iframe.
        event.target;
    });
</script>
</body></html>

// iframe.html
<script>
</script>
```

To make sure that a DOM wrapper is created in a correct context, you need to
make sure that the current context must be set to the correct context
whenever you call ToV8(). If you're not sure what context to use,
ask haraken@chromium.org.

