[adjustable_auction.cc] is a browsertest-based tool for running simulated 
auctions with custom command line parameters. The tool is useful for 
profiling hypothetical scenarios locally. 

To use the tool:
1. Ensure your gn build configuration has `is_debug = false` so that you 
produce a release build.
1. Build with `autoninja -C out/{target} adjustable_auction`.
1. Choose which parameters to use when configuring the auction. Most parameters
 control aspects of the auctions (e.g. `--n-auctions`, 
`--ads-per-ig`). Others are important to allow us to collect accurate 
latency information. `--hist-filename` is the filename to save UMA 
histograms from the auction. Set a large enough `--preauction-delay` to 
ensure all interest groups have already been joined before starting an 
auction. See `SetUpCommandLineArgs()` in [adjustable_auction.cc] for a full 
list of parameters and their descriptions.
1. If your auction is likely to take a long time, you may want to increase 
the test timeouts with `--ui-test-action-max-timeout=1000000` and  
`--test-launcher-timeout=1000000`.
1. This tool is particularly useful when used with the `perf` command line 
tool. A delay can be set with `-D` equal to the delay set with 
`--preauction-delay` to ensure that only the auctions are captured, not any of
the set up. The following is an example of running perf with the tool:
```
perf record -e cpu-clock -F 8000 -D 10000 -o perf_output_file.data -g out/{target}/adjustable_auction ---ozone-platform=headless --ads-per-ig=30 --n-auctions=1 --owners=2 --ig-per-owner=100 --scoring-delay=0 --preauction-delay=10 --bidding-delay=10 --hist-filename=output_histograms.txt
```

[adjustable_auction.cc]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/tools/adjustable_auction.cc
