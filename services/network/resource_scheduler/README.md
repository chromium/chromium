# Resource Scheduler

Resource scheduler controls the loading of the webpages by scheduling the order in which the HTTP requests are dispatched on the network. It ensures that the loading of lower priority resources (e.g., XHRs, images) does not slow down the loading of the higher priority resources (e.g., HTML, CSS, render blocking JavaScript) while ensuring the maximum usage of the available network resources.

## Design

Resource scheduler creates one resource scheduler client is created per execution context (aka per-frame and service worker). This means that iframes get their own scheduler client. Each scheduler client controls the loading of the requests relevat in its context. At a high level, the scheduling logic does not dispatch low priority requests on the network if they are expected to contend for network resources with higher priority requests. This contention is estimated based on the count of higher priority requests in-flight and the estimated network capacity.
