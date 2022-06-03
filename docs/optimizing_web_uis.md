# Optimizing Chrome Web UIs

## How do I do it?

In order to build with a fast configuration, try setting these options in your
GN args:

```
optimize_webui = true
is_debug = false
```

## How is the code optimized?

### Resource combination

[HTML imports](https://www.html5rocks.com/en/tutorials/webcomponents/imports/)
are a swell technology, but can be used is slow ways.  Each import may also
contain additional imports, which must be satisfied before certain things can
continue (i.e. script execution may be paused).

```html
<!-- If a.html contains more imports... -->
<link rel="import" href="a.html">
<!-- This script is blocked until done. -->
<script> startThePageUp(); </script>
```

To reduce this latency, Chrome uses a tool created by the Polymer project named
[polymer-bundler](https://github.com/Polymer/polymer-bundler).  It processes
a page starting from a URL entry point and inlines resources the first time
they're encountered.  This greatly decreases latency due to HTML imports.

```html
<!-- Contents of a.html and all its dependencies. -->
<script> startThePageUp(); </script>
```

### CSS @apply to --var transformation

We also use
[polymer-css-build](https://github.com/PolymerLabs/polymer-css-build) to
transform CSS @apply mixins (which are not yet natively supported) into faster
--css-variables.  This turns something like this:

```css
:host {
  --mixin-name: {
    color: red;
    display: block;
  };
}
/* In a different place */
.red-thing {
  @apply(--mixin-name);
}
```

into the more performant:

```css
:host {
  --mixin-name_-_color: red;
  --mixin-name_-_display: block;
}
/* In a different place */
.red-thing {
  color: var(--mixin-name_-_color);
  display: var(--mixin-name_-_display);
}
```

### JavaScript Minification

In order to minimize disk size, we run
[uglifyjs](https://github.com/mishoo/UglifyJS2) on all combined JavaScript. This
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

### Gzip compression of web resources

As of [r761031](https://chromium.googlesource.com/chromium/src/+/6b83ee683f6c545be29ee807c6d0b6ac1508a549)
all HTML, JS, CSS and SVG resources are compressed by default with gzip
Previously this was only happening if the `compress="gzip"` attribute was
specified as follows in the corresponding .grd file:

```xml
<include name="IDR_MY_PAGE" file="my/page.html" type="BINDATA" compress="gzip" />
```

This is no longer necessary, and should be omitted. Only specify the `compress`
attribute if the value is `false` or `brotli`.

Compressed resources are uncompressed at a fairly low layer within
ResourceBundle, and WebUI authors typically do not need to do anything special
to serve those files to the UI.
