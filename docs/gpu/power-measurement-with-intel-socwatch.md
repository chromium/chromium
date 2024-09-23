# How to Measure Power Using Intel SoC Watch Software

Intel SoC Watch is a command line tool for monitoring and debugging system behaviors related to power consumption on Intel architecture-based platforms. It reports active and low power states for the system/CPU/GPU/devices, processor frequencies and throttling reasons, wakeups, and other metrics that provide insight into the system’s energy efficiency. The tool includes utility functions that include delaying the start of collection and launching an application prior to starting collection.
Data is collected from both hardware and OS sources. When using the default mode of collection, the tool collects data at normally occurring OS context-switch points so that the tool itself is not perturbing the system sleep states. Tool overhead when collecting during idle scenarios can be < 1%, however active workloads with a high-rate of context switching will increase the overhead. A minimum collection interval is used to control the rate of collection.
Intel SoC Watch writes a summary report file (.csv) at the end of collection on the system under analysis (target system), allowing immediate access to results.

The following is now public documentation on how to achieve this:

[How to Deploy Intel SoC Watch and Measure Power.](https://docs.google.com/document/d/1mNOi95U4GfkgCaETXrrYnzaXvuFvsTSJ)

See also [Intel SoC Watch Command-line Tool Options](https://www.intel.com/content/www/us/en/docs/socwatch/user-guide/2023-1/intel-soc-watch-command-line-tool-options.html)
and [Intel SoC Watch for oneAPI Release Notes](https://www.intel.com/content/www/us/en/developer/articles/release-notes/oneapi-soc-watch.html) for more information.
