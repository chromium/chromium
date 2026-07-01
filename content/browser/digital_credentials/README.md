# Digital Credentials Browser-Side Implementation

This directory contains the browser-side implementation of the Digital
Credentials API.

## Testing Strategy

To test the Digital Credentials API (e.g., via Web Platform Tests), we need to
simulate different user behaviors (e.g., successful selection, dismissal,
aborting).

Since the real UI is platform-specific and requires user interaction, we use
fake implementations for testing.

### 1. Default Behavior (Hanging)

By default in test environments, we want the API calls
(`navigator.credentials.get()` and `navigator.credentials.create()`) to **hang**
(i.e., remain pending). This simulates the browser waiting indefinitely for the
user to interact with the UI.

This hanging behavior is crucial for testing:
*   **Aborts**: Testing that `AbortSignal` can cancel a pending request.
*   **Concurrency**: Testing that a second request is rejected if a first
    request is already pending.

To achieve this:
*   **Chrome**: The desktop provider (`DigitalIdentityProviderDesktop`)
    naturally hangs because it attempts to show a UI and waits for user
    interaction (which never happens in automated tests).
*   **Content Shell**: `ShellContentBrowserClient` overrides
    `CreateDigitalIdentityProvider` to return `ShellDigitalIdentityProvider`,
    which hangs by default (does not invoke the callback).
*   **Headless Shell**: `HeadlessContentBrowserClient` overrides
    `CreateDigitalIdentityProvider` to return `HeadlessDigitalIdentityProvider`,
    which also hangs by default.

### 2. Auto-Resolve Behavior (Fake UI)

For tests that expect a successful token retrieval (e.g., `wpt_internal`
tests), we can configure the fake UI to **auto-resolve** with a fake token after
1ms.

This is enabled by passing the command-line flag:
`--use-fake-ui-for-digital-identity`

When this flag is present:
*   `DigitalIdentityRequestImpl` bypasses the embedder's provider.
*   It immediately posts a task to resolve the request with a fake token.

This flag is used in the `digital-credentials-fake-ui` virtual test suite
(defined in `third_party/blink/web_tests/VirtualTestSuites`) for running
success path tests.
