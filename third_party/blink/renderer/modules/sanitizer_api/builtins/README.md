# Sanitizer API Builtins

The Sanitizer API makes use of several built-in constants, which are essential
for both compatibility, as well as for the security guarantees the API is meant
to offer to developers.

Previously, these were hard coded in our Sanitizer implementation. This
directory contains copies of these constants straight from the specification
document and generates the actual C++ constants from them with a fairly simple
script. The goal is to make it easy to update these constants as the
specification evolves, so that updating our implementation becomes a mere
copy & paste operation.

Constants are derived from:

: baseline_element_allow_list.txt
:: https://wicg.github.io/sanitizer-api/#baseline-elements
: baseline_attribute_allow_list.txt
:: https://wicg.github.io/sanitizer-api/#baseline-attributes
: default_configuration.txt
:: https://wicg.github.io/sanitizer-api/#default-configuration-object
