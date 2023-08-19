# Builder Health Indicators
Health indicators can be found on a builder's home page. They provide signal on a few different things like:
- Is it running in a reasonable amount of time?
- Is it persistently flaking/failing?
- Is it having capacity trouble?
- Is it having infra trouble?

If any of the [health criteria](https://source.chromium.org/chromium/infra/infra/+/main:go/src/infra/cr_builder_health/thresholds.go?q=f:thresholds.go%20%22type%20BuilderThresholds%22) is found to be unhealthy,
the builder is marked unhealthy. Follow the link in the UI to find more health info about that specific builder.

Health indicators can be turned on, turned off, and [configured](https://source.chromium.org/search?q=builder_health_indicators.star) alongside builder infra configs.
The config end result is this [json file](https://crsrc.org/c/infra/config/generated/health-specs/health-specs.json;l=1?q=health-spec&sq=).

If a builder is unhealthy it is the owning team's job to fix it, or remove it.
