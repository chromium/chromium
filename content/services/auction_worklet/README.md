# Auction Worklet Service

A central part of [FLEDGE](https://github.com/WICG/turtledove/blob/main/FLEDGE.md)'s
[running of on-device ad auctions](https://github.com/WICG/turtledove/blob/main/FLEDGE.md#2-sellers-run-on-device-auctions)
is the execution of
[bidding](https://github.com/WICG/turtledove/blob/main/FLEDGE.md#32-on-device-bidding)
and
[bid scoring](https://github.com/WICG/turtledove/blob/main/FLEDGE.md#23-scoring-bids)
worklets. These worklets execute JavaScript functions that do not have access to
networking, storage, or any other part of a web page (e.g. DOM).  Due to the
execution of downloaded code and these isolation requirements they're run in
a separate process.  The Auction Worklet Service orchestrates and controls the
execution of these auction worklets.  Before making changes to this service
please thoroughly consult the
[OWNERS](https://chromium.googlesource.com/chromium/src/+/main/content/services/auction_worklet/OWNERS)
to ensure changes do not violate the isolation requirements.
