The sandbox policy
------------------

The sandbox policy determines a set of capabilities a document will have.
It is defined in two types of objects:
- The frame.
- The document.

The frame policy is parsed from the <iframe>’s sandbox attribute.
Example: <iframe sandbox="allow-script allow-origin">

The document policy is parsed from the HTTP Content-Security-Policy header.
Example: Content-Security-Policy: sandbox allow-script allow-origin

On top of that, the sandbox policy is inherited from:
- The frame to its document.
- The document to its children frames.
- The document to its opened windows’s main frame.

The distinction between a frame and a document is important. The document is
replaced after navigations, but the frame stays.

The sandbox policy is bit field. The sandbox flags are defined by:
/services/network/public/mojom/web_sandbox_flags.mojom
Multiple sandbox policy are combined using a bitwise AND in the bitfield. This
way, the policy can only be further restricted.

Specification:
- http://www.whatwg.org/specs/web-apps/current-work/#attr-iframe-sandbox
