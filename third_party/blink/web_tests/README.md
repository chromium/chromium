The documentation for this directory is at:
- [Web Tests](/docs/testing/web_tests.md)
- [Writing Web Tests](/docs/testing/writing_web_tests.md)

## wptserve Configuration

`web_tests/external/wpt/config.tmpl.json` is a configuration file template for
overriding wptserve's default routes and ports.
wptserve's default ports conflict with those used by httpd for legacy web
tests, so the configuration file remaps them elsewhere.

Note that the contained filesystem paths are relative to `blink/web_tests/`.
At runtime, `run_{web,wpt}_tests.py` will resolve them to absolute paths so
that the serving behavior doesn't depend on its working directory.

When changing the ports for new servers, make sure to also:

- Update `WPT_HOST_AND_PORTS` in
  `//third_party/blink/tools/blinkpy/web_tests/port/driver.py`
- Update `WebTestContentBrowserClient::GetOriginsRequiringDedicatedProcess`
