If you're a Chrome developer, here's how to run this example:

Build the example, i.e. "make ppapi_example_url_loader" on Linux.

In the "src" directory, start the test server:

  On Linux:
    vpython net/tools/testserver/testserver.py --port=1337 --data-dir=ppapi/examples/url_loader

  On Windows:
    vpython net/tools/testserver/testserver.py --port=1337 --data-dir=ppapi/examples/url_loader

Then load the page:

  On Linux:
    out/Debug/chrome --register-pepper-plugins="[[[ YOUR SOURCE DIR ]]]/out/Debug/lib.target/libppapi_example_url_loader.so;application/x-ppapi-url-loader-example" http://127.0.0.1:1337/files/url_loader.html

  On Windows just substitute the generated .dll name for the .so.

And click the button!
