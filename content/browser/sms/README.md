# Web OTP API

Android has [automatic and one-tap SMS verification](https://developers.google.com/identity/sms-retriever). We would like to cover the gap on web platform and implement the WebOTP Service API for web developers.

## Web-exposed Interfaces

### [Web OTP API](https://github.com/WICG/WebOTP)

This is implemented in [third_party/blink/renderer/modules/credentialmanagement](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/credentialmanagement/) and exposes the following function:

```navigator.credentials.get({otp: {transport: ["sms"]}})```

## Testing

* Unit tests are located in [content/browser/sms/webotp_service_unittest.cc](https://cs.chromium.org/chromium/src/content/browser/sms/webotp_service_unittest.cc).
* Browser tests are located in [content/browser/sms/sms_browsertest.cc](https://cs.chromium.org/chromium/src/content/browser/sms/sms_browsertest.cc).
* The Android related tests are located in [chrome/android/javatests/src/org/chromium/chrome/browser/sms/](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/sms/).
* Web platform tests are located in [third_party/blink/web_tests/http/tests/credentialmanagement/](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/http/tests/credentialmanagement/)

For how to run these tests, refer to Chromium documentation [Running tests locally](https://www.chromium.org/developers/testing/running-tests), [Android Test Instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/android_test_instructions.md)

For testing this API locally, refer to [How to use the Web OTP API](https://github.com/WICG/WebOTP/blob/master/HOWTO.md)

## Platform Support

### Android

The Android implementation (this does not include Android WebView) is located in [chrome/browser/ui/android/sms/](https://cs.chromium.org/chromium/src/chrome/browser/ui/android/sms/), [content/public/android/java/src/org/chromium/content/browser/sms/](https://cs.chromium.org/chromium/src/content/public/android/java/src/org/chromium/content/browser/sms/), and [chrome/android/java/src/org/chromium/chrome/browser/sms/](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/sms/).

For architectural diagrams, refer to [design doc](https://docs.google.com/document/d/1dB5UM9x8Ap2-bs6Xn0KnbC_B1KNLIUv4W05MunuXYh0).

### Desktop

We plan to implement it in the near future.

## Related Documentation

[Design Doc](https://docs.google.com/document/d/1dB5UM9x8Ap2-bs6Xn0KnbC_B1KNLIUv4W05MunuXYh0)

[Web OTP API explainer](https://github.com/WICG/WebOTP)

[How to use the Web OTP API](https://github.com/WICG/WebOTP/blob/master/HOWTO.md)

[Launch Bug](https://bugs.chromium.org/p/chromium/issues/detail?id=670299)
