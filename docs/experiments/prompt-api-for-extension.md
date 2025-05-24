# Prompt API for extension

This document describes the status of the current implementation of the
[**Prompt API**](https://github.com/webmachinelearning/prompt-api) for Chrome
extensions, and how to verify.

## What’s supported

The implementation generally intends to follow the
[explainer](https://github.com/explainers-by-googlers/prompt-api).

## Activation

The API can be enabled by participating in the
[extension origin trial](https://developer.chrome.com/blog/prompt-api-origin-trial)
named `AIPromptAPIForExtension`. After obtaining the trial token, the
extension authors need to configure it in the `manifest.json`:

```json
{
  "trial_tokens": [<GENERATED_TOKEN>],
}
```

## Verifying the API is working

The extension authors can verify if the API is available by checking for the
presence of the `LanguageModel` entrypoint from extension window and worker
scripts. If the object is defined, the authors can follow the
[explainer](https://github.com/explainers-by-googlers/prompt-api) and
[developer docs](https://developer.chrome.com/docs/extensions/ai/prompt-api)
to check availability and test the APIs usage.

## Related Links

- [Explainer on GitHub](https://github.com/webmachinelearning/prompt-api)
- [API feedback](https://github.com/webmachinelearning/prompt-api/issues)
- [Reporting bugs](https://issues.chromium.org/issues/new?component=1583624)
- [Extension origin trial](https://developer.chrome.com/blog/prompt-api-origin-trial)
- [Developer docs](https://developer.chrome.com/docs/extensions/ai/prompt-api)
