# Mixed content Autoupgrade

## Description
Chrome will now (starting on M80) attempt to upgrade some types of mixed content (HTTP on an HTTPS site) subresources. Subresources that fail to load over HTTPS will not be loaded. For more information see [the official announcement](https://blog.chromium.org/2019/10/no-more-mixed-messages-about-https.html).

## Scope
Audio, video, and image subresources are upgraded. Blockable (i.e. all other types of) mixed content are blocked without an autoupgrade attempt.

## Opt-out
Users can disable autoupgrades, and allow blockable mixed content to load, on a per-site basis through content settings (chrome://settings/content/insecureContent).
