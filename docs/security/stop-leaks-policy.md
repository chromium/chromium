# Stop leaks policy

## What leaks?
XS-Leaks ([cross-site leaks](https://xsleaks.dev/)) are a type of attack where an attacker web page is able to bypass the same-origin policy and gain access to data in sensitive web applications to which the user is logged in. XS-Leaks do not require the attacker to gain code execution in the browser, and can be achieved entirely through the use of legitimate web APIs.

Any new XS-Leak we might introduce to the web platform has the potential to regress the security of all existing web applications by creating a novel way to leak data. For web applications that try to protect themselves against known XS-Leaks, any new way of leaking data means that they must secure themselves against the leak, incurring extra cost to develop and deploy web-level mitigations. In some cases, websites might not even be aware of—or able to—deploy a mitigation by themselves, and will be dependent on protections built into the web platform to ensure they don't leak data to other websites.

This core issue is compounded by the fact that even minor leaks in the web platform can often be abused to reveal sensitive data about actual applications. For example, inferring whether a given resource had been in the HTTP cache sounds fairly benign. But in practice this can allow XS-Search and reveal sensitive data when exploited against users of e.g. a  [webmail application](https://www.blackhat.com/docs/us-16/materials/us-16-Gelernter-Timing-Attacks-Have-Never-Been-So-Practical-Advanced-Cross-Site-Search-Attacks.pdf) or a [map application](https://www.comp.nus.edu.sg/~prateeks/papers/Geo-inference.pdf).

**It is our goal to move the Web Platform towards a future with stronger cross-site boundaries that protect applications from exposing data about logged in users to other websites, which is critical for long term trust in the security of the platform.**

## Stop leaks policies

### Web API design

When designing a new Web API, pay attention to the following:

#### Memory information
**APIs providing information about memory usage should be gated behind crossOriginIsolation.**

> Reasoning: APIs that provide information about memory usage can be used to leak information about the size of cross-origin resources, bypassing the same-origin policy. [CrossOriginIsolation](https://developer.mozilla.org/en-US/docs/Web/API/crossOriginIsolated) is designed to deal with this threat model.

> Example: [performance.measureUserAgentSpecificMemory()](https://developer.mozilla.org/en-US/docs/Web/API/Performance/measureUserAgentSpecificMemory) is gated behind [CrossOriginIsolation](https://developer.mozilla.org/en-US/docs/Web/API/crossOriginIsolated).

#### Timers
**Precise timers should be gated behind [CrossOriginIsolation](https://developer.mozilla.org/en-US/docs/Web/API/crossOriginIsolated).**

> Reasoning: Precise timers can be used to mount efficient Spectre attacks, allowing to read cross-origin resources, bypassing the same-origin policy. [CrossOriginIsolation](https://developer.mozilla.org/en-US/docs/Web/API/crossOriginIsolated) is designed to deal with this threat model.

> Example: SharedArrayBuffers can be used to build precise timers, and thus are gated behind CrossOriginIsolation.

#### CPU information
**Do not expose CPU global utilization beyond the [Compute Pressure API pressure states](https://www.w3.org/TR/compute-pressure/), and do not expose thread specific or document specific CPU utilization information.**

> Reasoning: APIs that expose CPU global utilization can be used to leak activity of cross-origin documents, including pages in other processes. This bypasses the same-origin policy and Site Isolation.

> Example: the [Compute Pressure API](https://www.w3.org/TR/compute-pressure/) offers a reasonable tradeoff between usability and security. An API with more buckets of CPU utilization would risk leaking too much information.

#### Global state
**Any state maintained by the API must be partitioned according to the Network or Storage key.**

> Reasoning: APIs that depend on a global state (like unpartitioned history) can be used to leak the interactions of cross-origin resources with that global state, bypassing the same-origin policy.

> Example: the [CSS :visited property](https://developer.mozilla.org/en-US/docs/Web/CSS/:visited) applied whenever a user had visited any link, allowing to leak the user’s complete browsing history. Instead, it should be partitioned so that it only applies to links a user clicked on from a particular network partition.

### Web platform implementation
When implementing a new web-observable behavior, pay attention to the following:

#### CPU information
**Use only coarse-grained, web exposed CPU utilization data (i.e. [Compute Pressure API pressure states](https://www.w3.org/TR/compute-pressure/)) to make implementation decisions.**

> Reasoning: web-observable implementation choices can be leveraged by an attacker in the same manner as APIs exposing CPU utilization directly. An attacker can use this to learn information about the activity of cross-origin documents, including in other processes.

> Example: freezing background pages due to CPU pressure can be detected by an attacker. If the freezing thresholds are different from the web-exposed Compute Pressure API, this gives an attacker additional info about CPU utilization.

#### Global state
**Do not rely on any global state that is not partitioned according to the Network or Storage key. In particular, avoid designing any limits scoped to the tab or top-level window; instead, rely on document-level limits.**

> Reasoning: limits that are global can be exploited by an attacker to learn how other cross-origin resources are interacting with this limit.

> Example: having a [global limit for the number of sockets](https://xsleaks.dev/docs/attacks/timing-attacks/connection-pool/) allows an attacker to exhaust the socket pool. The attacker can then open a new page and progressively release sockets, then attempt to claim them back, gaining information about the number of network connections opened by the other page.

#### Personalization
**Do not use the presence or absence of cookies or history data to make implementation decisions beyond what is specified in the web platform.**

> Reasoning: using personalized data to make implementation choices can allow an attacker to gain access to this data.

> Example: triggering a preload of a page based on whether a user has cookies for the page’s site allows an attacker to learn whether a user had cookies for another site or not.

## FAQ
**Can’t I just add some noise to mitigate the issue?**
Unfortunately, research has shown that it’s possible to bypass noise added to a side-channel to get to the underlying data. So adding noise is not a sound security mitigation.

**Is this everything I need to worry about when it comes to XS-Leaks?**
No, there are a number of less common pitfalls that might introduce the risk of cross-site leaks, which we might point out during security reviews of your feature. This is one of the reasons why most features should go through security review.

**Do you have guidance on other web-related security risks beyond XS-Leaks?**
Yes, we have more general [web platform guidelines](https://chromium.googlesource.com/chromium/src/+/master/docs/security/web-platform-security-guidelines.md), and also a [questionnaire](https://chromium.googlesource.com/chromium/src/+/master/docs/security/web-platform-security-questionnaire.md) to help you understand whether your web API might have security impacts.
