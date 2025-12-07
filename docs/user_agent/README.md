## User-Agent

The User-Agent request header is a characteristic string that allows servers and
network peers to identify the application, operating system, vendor, and/or
version of the requesting user agent.

Because the User-Agent string carries a significant amount of data that can be
used for passive fingerprinting, Chrome has been fully rolling out User-Agent
reduction since Chrome M110. This aims to minimize the information in the
User-Agent string to enhance user privacy. For more details, please refer to the
[User-Agent Reduction documentation](https://www.chromium.org/updates/ua-reduction/).

### Default User-Agent

Modifying the default User-Agent format can lead to requests being rejected by
servers; therefore, any changes must be made with caution. As new device types
emerge, it is crucial to follow existing patterns when altering the default
User-Agent rather than creating a custom format.

Currently, different versions of Chrome utilize the central API
`embedder_support::GetUserAgent()` to construct platform-specific User-Agent
strings. This function employs `BUILD_FLAG`s to differentiate between builds for
various platforms such as Android, Windows, and macOS. For platform variants,
like iPhone or iPad, `ui::GetDeviceFormFactor()` is used to distinguish between
device types and compile the appropriate User-Agent string into the binary.

**Note:** User-Agent reduction has not been enabled for Chrome on Apple devices.
For all other devices, please ensure that no build model or detailed operating
system version information is included in the User-Agent string.

If you have any questions regarding how to change the default User-Agent, please
reach out to victortan@chromium.org, miketaylr@chromium.org.

### User-Agent Client Hints

With the implementation of User-Agent reduction, we have introduced User-Agent
Client Hints. This feature enables developers to actively request specific
information about a user's device or conditions, eliminating the need to parse
this data from the User-Agent string. For additional context, see the
[User-Agent Client Hints developer
documentation](https://developer.chrome.com/docs/privacy-security/user-agent-client-hints)
and the [User-Agent Client Hints
Specification](https://wicg.github.io/ua-client-hints/).

Changes to the default User-Agent for different platforms typically require
corresponding changes to the User-Agent Client Hints. For instance, if the
User-Agent is changed from "Android" to "Linux," the `sec-ch-ua-platform` client
hint must also be updated accordingly. To understand how default User--Agent
Client Hints are generated, please review
`embedder_support::GetUserAgentMetadata` in
`components/embedder_support/user_agent_utils.cc`.

*   To add a new client hint, please follow [this
    guide](components/client_hints/README.md).
*   To fully understand the lifecycle of a client hint, refer to [this
    document](/docs/client_hints/README.md).

### User-Agent Overrides

User-Agent overrides can originate from two different sources: the browser
process via `WebContentsImpl::SetUserAgentOverride` and DevTools.

For renderer or subresource requests, the override behavior depends on
`CommitNavigationParams.is_overriding_user_agent`, which is set in the browser
process.

To access the User-Agent override values, read the `user_agent_override`
property from `blink::RendererPreferences`. This property sets the User-Agent
override from the browser process, and then a DevTools-provided method is called
to apply the DevTools override.

Intercepting and modifying User-Agent overrides for every possible request type
is a non-trivial task. Currently, there is no specific team or individual
responsible for the User-Agent override behavior. Generally, feature teams are
responsible for ensuring that the override functionality works as expected.
There are cases where User-Agent overrides may not function as intended. Please
ensure that supporting User-Agent overrides does not introduce any security
vulnerabilities.
