This directory implements the Blink side of the [Web Share
API](https://www.w3.org/TR/web-share/).

It needs to be hooked up to a corresponding Mojo service in the browser, which
will typically be different for each operating system, given that it needs to
interface with that OS's native share system.
