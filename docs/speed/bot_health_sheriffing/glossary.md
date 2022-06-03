# Bot health sheriff glossary

* **Alert: **An row in Sheriff-o-matic that indicates a problem affecting a test.

* **Benchmark: **A set of test cases that measure some aspect of Chrome's performance. The most common type of benchmark that we manage is Telemetry benchmarks.

* **Bisect: **A binary search within a range of Chrome commits to find which the revision responsible for a functional or performance regression. Pinpoint is the name of the bisection microservice.

* **Bot: **An ambiguous term that can mean either *swarming bot* or *host*.

* **Device: **The physical hardware that we run performance tests on.

* **Flakiness dashboard: **[A dashboard](https://test-results.appspot.com/dashboards/flakiness_dashboard.html#testType=blink_perf.layout) that shows the revisions at which a given test failed. Also known as the test results dashboard.

* **Host: **The physical hardware that Telemetry runs on. For desktop testing, this is the same as the *device* on which the testing is done. For mobile testing, the *host* can mean either the Linux desktop or one of the multiple Docker containers within that Linux desktop, each with access to a single attached mobile device.

* **Hostname: **A string identifier for the host which looks like "build139-b1". For mobile testing, there's both a hostname for the Linux desktop (e.g. "build140-b1") and a hostname for each Docker container within that host (e.g. "build140-b1--device6").

* **Isolate: **A container sent to a swarming bot containing everything that a swarming task needs to run. (Pronounced "ice-o-lit".)

* **Logdog: **The microservice within LUCI that displays logs.

* **LUCI: **An acronym standing for **"**Layered Universal Continuous Integration". It is a collection of scalable open-source build infrastructure services that includes microservices like swarming and LogDog.

* **Pinging a bug: **To add a comment on a bug asking if there's any status update. Generally this is done when we expected some action to have been taken towards resolving the bug, but it wasn't.

* **Pinging a person [offline]**: To contact someone on chat. Generally, this is done to follow up on a bug that's particularly urgent or that needs attention from someone who has been unresponsive on Monorail.

* **Pinpoint: **[The microservice](https://pinpoint-dot-chromeperf.appspot.com/) used to run bisects.

* **Sherff-o-matic: **[The central tool](https://sheriff-o-matic.appspot.com/chromium.perf#) used to bot health sheriff.

* **Snooze: **An action taken by sheriffs to dismiss an alert for a defined amount of time.

* **Story: **A single test case within a Telemetry benchmark.

* **Swarming: **A set of AppEngine services used to distribute and run tasks on a pool of hosts.

* **Swarming bot: **A Python binary running on each host that polls for new swarming tasks and executes them.

* **Swarming task: **A command that a given swarming bot is told to run (e.g. "Run this Telemetry Python script with these parameters.").

* **Telemetry: **The infrastructure used to write Chrome performance benchmarks.

