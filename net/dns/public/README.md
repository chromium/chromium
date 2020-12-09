# DNS Public

Host resolution code intended for direct usage outside the network stack. This
should be limited to simple utilities, structs, and constants. While code
designed to work with the [network service](/services/network) should generally
only interact with host resolution through the service, the code in this
directory is designed to be used directly by any code.

TODO(ericorth): Move to //net/public/dns if the cleanup is ever started to
generally separate public code in //net.

## Adding/Modifying DoH providers

See [adding_doh_providers.md](/net/docs/adding_doh_providers.md).
