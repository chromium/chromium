This directory contains configuration for dealing with outages.

In response to outages, it is occasionally necessary to disable or alter the
behavior of some portions of the LUCI project. The files located within this
directory provide facilities to easily make such changes without having to
modify the definitions themselves.

# Directory contents

* **config.star**: The file containing the actual outages configuration. See
  [Modifying the outage configuration](#modifying-the-outages-configuration) for
  more information.
* **outages.star**: The file containing the generators that implement the
  outages behavior.

# Modifying the outages configuration

Within config.star, `config` is declared with the result of a function call. To
change the outages configuration you can modify the parameters to the call. The
available parameters are:

* `disable_cq_experiments`: (default: False) Disable experimental tryjobs on the
  chromium CQ. This can be used to save both builder and test capacity when the
  CQ is capacity-constrained. The exact amount and configurations of capacity
  that is freed up depends on the specifics of what builders have experimental
  tryjobs and what experiment percentage is used, but disabling all CQ
  experiments will quickly free up any capacity that might be instead used for
  the CQ so that we can focus on trying to address the root cause or implement
  other workarounds to improve the CQ experience for developers rather than
  tracking down which experimental tryjobs would be using the capacity that is
  needed for the particular outage.
