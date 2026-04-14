# Blink extensions for Isolated Web Apps in ChromeOS

This directory defines the `chromeos.isolatedWebApp` interface containing APIs
restricted to allowlisted Isolated Web Apps running in Chrome OS.

## Requesting access

If you are developing an Isolated Web App and require access to a blink
extension please follow these steps to request access:

1.  Use the standard
    [IWA allowlist request process](https://developer.chrome.com/docs/iwa/allowlist).
2.  In the request form, locate the question "Which IWAs specific API's are
    needed and why?".
3.  In your answer to this question, state which blink extension you require
    access to. Provide a detailed justification with context about your
    application and the features for which you need the blink extension.

The request will be evaluated as part of the overall IWA allowlist review
process. Please note that access to these blink extensions is not guaranteed and
is subject to review.
