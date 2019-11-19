# Mixed content Autoupgrade

## Description
We are currently running an experiment upgrading mixed content (insecure content on secure sites) to HTTPS, as part of this, some users will see HTTP subresource URLs rewritten as HTTPS when browsing a site served over HTTPS. This is similar behavior to that if the site included the Upgrade-Insecure-Requests CSP directive.

## Scope
Currently subresources loaded over HTTP and Websocket URLs are autoupgraded for users who are part of the experiment. Form submissions are not currently part of the experiment.

## Opt-out
You can opt out of having mixed content autoupgraded in your site by including an HTTP header with type 'mixed-content' and value 'noupgrade', this will disable autoupgrades for subresources. Since mixed content websockets are automatically blocked, autoupgrades cannot be disabled for those.
