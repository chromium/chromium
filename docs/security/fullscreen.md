# Chrome Fullscreen Security FAQ

## Overview
The Fullscreen API inherently presents a security challenge by granting websites full display control, potentially leading to user uncertainty about their fullscreen status. The reported fullscreen bugs primarily represent edge cases of this existing state rather than discovering new security vulnerabilities. In reality, malicious sites are taking advantage of this state, without exploiting such edge cases. Fixing these isolated states would not address the core concerns. So, most of the reported bugs should be categorized as low severity or functionl bugs.

We will focus on enhancing the overall security and user experience of fullscreen mode rather than addressing individual edge cases. Specifically, we will prioritize improvements to the fullscreen toast UI and ensure the trusted browser UI is shown in sensitive security scenarios to help users make more informed decisions. Such strategy allows us to provide a more robust and secure fullscreen experience for all users. Progress can be tracked in [issue 391919449](https://crbug.com/391919449).

## FAQ

### Is it a security bug that the fullscreen toast "Press [Esc] to exit fullscreen" is being obscured by other UI elements?

It depends, but at most this would be a low-severity security issue.

While the severity [guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md#toc-medium-severity) say "A bug that allows web content to tamper with trusted browser UI", partial obfuscation of the fullscreen toast is not at all considered a security issue. The full obscuration of the fullscreen toast does not directly pose a security risk and is not harmful unless combined with other tactics.

Furthermore, the fullscreen toast, while useful, is not the only indicator of fullscreen mode. Chrome provides other visual cues like the fullscreen animation to inform users of the fullscreen state change. While obscuring the fullscreen toast can lead to user confusion, this issue in and of itself is not considered a vulnerability.

### When an Esc key press event is used to dismiss other browser UI instead of exiting fullscreen mode, is this considered a security bug?

No, this behavior is working as intended and is not considered a security vulnerability.

The Esc key is a common and expected mechanism for dismissing various transient UI elements, such as modal dialogs, menus, or context-sensitive overlays, within the browser environment. In many cases, these UI elements may take precedence over a direct fullscreen exit. Also, users will always be able to exit fullscreen by continuing to press the Esc key.

### Sometimes websites do [something…], and users might believe that they have exited the fullscreen mode, while they have not. Is this considered a security bug?

This is a security vulnerability in the nature of the fullscreen experience, not specific to an individual site’s implementation. It’s difficult for users to figure out whether they are in fullscreen mode or not because the website can control all pixels on the screen. While specific site tactics can contribute to this confusion, they only marginally impact the overall effectiveness of such attacks. So addressing one specific bug does not protect users from such attacks.

