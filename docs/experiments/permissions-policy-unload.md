# Permissions-Policy: Unload

Contact: bfcache-dev@chromium.org

This document describes the status of the current implementation of the
[Permissions-Policy: unload feature](https://github.com/w3c/webappsec-permissions-policy/issues/444)
in Chrome, and how to enable it.

Starting from version 107,
Chrome experimentally supports Permissions-Policy: unload.
This allows sites to prevent usage of unload event listeners.

Note that this policy is not available by default.
Chrome plans to do an origin trial
to evaluate its effectiveness
and to allow site authors to give feedback.

## What’s supported

A new [permission](https://github.com/w3c/webappsec-permissions-policy/blob/main/permissions-policy-explainer.md) is added, `unload`,
which defaults to be enabled
but when disabled makes calls to `window.addEventListener("unload", callback)`
a no-op.

## What’s not supported


## Activation

The policy can be enabled in several ways.

### Using command line flag

Pass the `--enable-features=PermissionsPolicyUnload` command line flag.

### Using the Chromium UI

Enable the flag chrome://flags/#enable-experimental-web-platform-features .

### Using Origin Trial

The [Origin Trial](https://developer.chrome.com/blog/origin-trials/) feature is named `PermissionsPolicyUnload`.
Registration for the trial is [here](https://developer.chrome.com/origintrials/#/view_trial/1012184016251518977).
See [feature status](https://chromestatus.com/feature/5760325231050752) to find out
if the origin trial is currently running.

The [origin trial tutorial](https://developer.chrome.com/docs/web-platform/origin-trials/#take-part-in-an-origin-trial) describes how to participate.

### Verifying the API is working

In devtools, run the following piece of javascript

```js
document.createElement("iframe").allow="unload";
```

If it succeeds then the feature is enabled.
If it gives an error like
`Unrecognized feature: 'unload'.`
then the feature is not enabled.

## Related Links

- [Chrome Status](https://chromestatus.com/feature/5760325231050752)
- [Permissions-Policy: unload Explainer](https://github.com/fergald/docs/blob/master/explainers/permissions-policy-unload.md)
- [Permissions-Policy: Explainer](https://github.com/w3c/webappsec-permissions-policy/blob/main/permissions-policy-explainer.md)
