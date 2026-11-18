# Optimizing Chrome Web UIs

## How do I do it?

In order to build with a fast configuration, try setting these options in your
GN args:

```python
optimize_webui = true
is_debug = false
```

## How is the code optimized?

### Bundling

[JS Modules](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Modules)
are used heavily throughout the code. Fetching all imports and their transitive
dependencies can be slow, especially when there are too many requests during
initial page load.

To reduce this latency, Chrome uses [Rollup](https://rollupjs.org) to bundle the
code into a couple JS bundle files (usually one or two). This greatly decreases
latency of initial load, by eliminating the overhead that is associated with
each individual request.

### JavaScript Minification

In order to minimize disk size, we run
[terser](https://github.com/terser/terser) on all combined JavaScript. This
reduces installer and the size of resources required to load to show a UI.

Code like this:

```js
function fizzBuzz() {
  for (var i = 1; i <= 100; i++) {
    var fizz = i % 3 == 0 ? 'fizz' : '';
    var buzz = i % 5 == 0 ? 'buzz' : '';
    console.log(fizz + buzz || i);
  }
}
fizzBuzz();
```

would be minified to:

```js
function fizzBuzz(){for(var z=1;100>=z;z++){var f=z%3==0?"fizz":"",o=z%5==0?"buzz":"";console.log(f+o||z)}}fizzBuzz();
```

If you'd like to more easily debug minified code, click the "{}" prettify button
in Chrome's developer tools, which will beautify the code and allow setting
breakpoints on the un-minified version.

### Compression of web resources

All HTML, JS, CSS, SVG, and JSON resources are compressed by default with zstd.
Only specify the `compress` attribute to override the default (for example
`false`, `brotli`, or a custom zstd level such as `zstd19`).

Compressed resources are uncompressed at a fairly low layer within
ResourceBundle, and WebUI authors typically do not need to do anything special
to serve those files to the UI.
