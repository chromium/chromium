# Attribution

`//services/network/attribution` contains logic supporting the [Attribution
Reporting API](https://github.com/WICG/attribution-reporting-api) in the network
service. Specific use cases are detailed below.

## Attribution Attestation

The directory contains logic that orchestrates [Trigger
Attestation](https://github.com/WICG/attribution-reporting-api/blob/main/trigger_attestation.md).
Hooked in the url_loader when applicable, it adds and parse specific headers
which allow reporting origins to submit a token attesting to the veracity of
a trigger registration.
