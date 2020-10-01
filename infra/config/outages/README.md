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

# Making changes during an outage

If you attempt to make a configuration change while an outages configuration is
enabled then the actual effect of your change may not be apparent if the outages
configuration impacts the portion of the configuration that you are modifying.
To prevent someone from unknowingly making such a change, a presubmit check will
prevent changes from being landed if there is an outages configuration in effect
unless all of the following are true:

* 1 or more LUCI configuration files within //infra/config/outages are modified.
* 0 or more LUCI configuration files within //infra/config/generated are
  modified.
* 0 other LUCI configuration files within //infra/config are modified.

If an outages configuration is in effect and you need to make a change that
doesn't meet these conditions to try and address the outage, add the footer
`Infra-Config-Outage-Action`. The value for the footer should be some link that
provides context on the outage being addressed.

If an outages configuration is in effect and you need to make an unrelated
change that cannot wait until the outage has been addressed, add the footer
`Infra-Config-Ignore-Outage`. The value for the footer should explain why it is
necessary to make such a change.
