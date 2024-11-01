# Prompt API for extension

- Contact: builtin-ai@chromium.org

This document describes the status of the current implementation of the
[**Prompt API**](https://github.com/explainers-by-googlers/prompt-api)
for Chrome extension, and how to verify.

Note that this API is not available by default. Chrome plans to do an
extension origin trial starting from M131 to evaluate its effectiveness
and to allow extension authors to give feedback.


## What’s supported

The API is implemented according to the
[explainer](https://github.com/explainers-by-googlers/prompt-api), but the
API namespace will be under `chrome.aiOriginTrial.languageModel` for the
extension origin trial.

## Activation

The API can be enabled by participating in the
[extension origin trial](https://developer.chrome.com/docs/web-platform/origin-trials#extensions)
named `AIPromptAPIForExtension`. After obtaining the trial token, the
extension authors need to configure it in the `manifest.json` together with
the `aiLanguageModelOriginTrial` permission.

```json
{
  "permissions": ["aiLanguageModelOriginTrial"],
  "trial_tokens": [<GENERATED_TOKEN>],
}

```

## Verifying the API is working

The extension authors can verify if the API is available by checking the
`chrome.aiOriginTrial.languageModel` from the service worker script.
If the `AILanguageModel` object is defined, the authors can follow the
[explainer](https://github.com/explainers-by-googlers/prompt-api) to test
the APIs usage.

## Related Links

- [Prompt API Explainer on GitHub](https://github.com/explainers-by-googlers/prompt-api)
- [Reporting bugs](https://g-issues.chromium.org/issues/new?component=1583624&priority=P2&type=feature_request&template=0&noWizard=true)
- [API Feedback](https://github.com/explainers-by-googlers/prompt-api/issues)
<!-- TODO: link the DevRel doc with more details once it's published -->
