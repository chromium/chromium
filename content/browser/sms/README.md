# SMS Receiver API

Android has [automatic and one-tap SMS verification](https://developers.google.com/identity/sms-retriever). We would like to cover the gap on web platform and implement the SMS Receiver API for web developers.

## Web-exposed Interfaces

### [SMS Receiver API](https://github.com/samuelgoto/sms-receiver)

This is implemented in [third_party/blink/renderer/modules/sms](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/sms/) and exposes the following function:

```navigator.sms.receive()```

## Testing

* Unit tests are located in [content/browser/sms/sms_service_unittest.cc](https://cs.chromium.org/chromium/src/content/browser/sms/sms_service_unittest.cc).
* Browser tests are located in [content/browser/sms/sms_browsertest.cc](https://cs.chromium.org/chromium/src/content/browser/sms/sms_browsertest.cc).
* The Android related tests are located in [chrome/android/javatests/src/org/chromium/chrome/browser/sms/](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/sms/).
* Web platform tests are located in [third_party/blink/web_tests/external/wpt/sms/](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/external/wpt/sms/) and are a mirror of the [sms web-platform-tests GitHub repository](https://github.com/web-platform-tests/wpt/tree/master/sms).

For how to run these tests, refer to Chromium documentation [Running tests locally](https://www.chromium.org/developers/testing/running-tests), [Android Test Instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/testing/android_test_instructions.md)

For testing this API locally, refer to [How to use the SMS Receiver API](https://github.com/samuelgoto/sms-receiver/blob/master/HOWTO.md)

## Platform Support

### Android

The Android implementation (this does not include Android WebView) is located in [chrome/browser/ui/android/sms/](https://cs.chromium.org/chromium/src/chrome/browser/ui/android/sms/), [content/public/android/java/src/org/chromium/content/browser/sms/](https://cs.chromium.org/chromium/src/content/public/android/java/src/org/chromium/content/browser/sms/), and [chrome/android/java/src/org/chromium/chrome/browser/sms/](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/sms/).

For architectural diagrams, refer to [design doc](https://docs.google.com/document/d/1dB5UM9x8Ap2-bs6Xn0KnbC_B1KNLIUv4W05MunuXYh0).

### Desktop

We plan to implement it in the near future.

## Related Documentation

[Design Doc](https://docs.google.com/document/d/1dB5UM9x8Ap2-bs6Xn0KnbC_B1KNLIUv4W05MunuXYh0)

[SMS Receiver API explainer](https://github.com/samuelgoto/sms-receiver)

[How to use the SMS Receiver API](https://github.com/samuelgoto/sms-receiver/blob/master/HOWTO.md)

[Launch Bug](https://bugs.chromium.org/p/chromium/issues/detail?id=670299)
