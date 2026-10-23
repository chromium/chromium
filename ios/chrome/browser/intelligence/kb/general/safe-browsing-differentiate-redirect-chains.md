---
name: ios/safe-browsing-differentiate-redirect-chains
description: When processing SafeBrowsing redirect chains, evaluate pending_main_frame_query_ only on pending chains, verify overall decisions before downstream promotion, and clear destination containers before transfer loops
rubric: correctness
severity: should_fix
citations:
  - path: docs/navigation_concepts.md
    heading: Rules for Canceling Navigations
    quote: Keep in mind that it is **not safe to simulate a redirect by canceling a navigation and starting a new one**, since this may lose relevant context from the original navigation (e.g., ReloadType, CSP state, Sec-Fetch-Metadata state, redirect chain, etc)
---

**Rule.** In `SafeBrowsingTabHelper::PolicyDecider`, properly isolate and manage
navigation stages across redirect chains (`pending_main_frame_redirect_chain_`,
`to_be_committed_redirect_chain_`, and `committed_redirect_chain_`):

1. **Decision Evaluation**: In `RedirectChainDecisionWithFilter`, check
   `pending_main_frame_query_` strictly when evaluating
   `RedirectChain::kPendingMainFrame`; never evaluate it for committed or
   to-be-committed chains.
2. **Chain Promotion and Lifecycle**: Before promoting queries from
   `pending_main_frame_redirect_chain_` to `to_be_committed_redirect_chain_`,
   verify whether an overall decision has been reached. Clear the pending chain
   if resolved, and only promote downstream while async checks remain pending.
   Additionally, ensure disallowed or cancelled navigation branches explicitly
   clear `pending_main_frame_redirect_chain_` when
   `UpdateToBeCommittedRedirectChain()` is conditional on navigation approval.
3. **Container Clearing and Transfers**: Do not prematurely clear
   `to_be_committed_redirect_chain_` when resetting pending chains in
   `ShouldAllowResponse`; update or clear it only when the navigation proceeds.
   When staging or committing queries (`UpdateToBeCommittedRedirectChain` or
   `SetCommittedRedirectChain`), clear destination containers once before
   entering the transfer loop rather than inside the loop body or omitting the
   clear.

**Why.** WebKit navigation stages advance independently while asynchronous Safe
Browsing checks execute in the background. Evaluating
`pending_main_frame_query_` on committed or to-be-committed redirect chains
attributes unsafe decisions from an in-flight navigation to the current safe
page, causing unintended reloads or blocking safe pages instead of the pending
navigation. Prematurely promoting pending queries downstream leaks decided
queries while unresolved queries remain pending, and skipping pending chain
cleanup on disallowed branches corrupts subsequent navigation policy checks with
stale queries. Furthermore, prematurely clearing
`to_be_committed_redirect_chain_` discards valid security state before
navigation proceeds, while clearing destination containers inside transfer loops
discards earlier iterations and corrupts the transferred redirect chain.

## Examples

Bad:

```cpp
// 1. Evaluating pending query on committed or to-be-committed chains:
if (pending_main_frame_query_->decision &&
    pending_main_frame_query_->decision->ShouldCancelNavigation()) {
  return pending_main_frame_query_->decision;
}

// 2. Promoting queries without checking overall decision or omitting clear:
if (decision->ShouldAllowNavigation()) {
  UpdateToBeCommittedRedirectChain();
}

// 3. Prematurely clearing to-be-committed chain in ShouldAllowResponse:
if (previous_main_frame_query_) {
  pending_main_frame_redirect_chain_.clear();
  to_be_committed_redirect_chain_.clear();
}

// 4. Clearing destination container inside transfer loop:
void SafeBrowsingTabHelper::PolicyDecider::UpdateToBeCommittedRedirectChain() {
  for (auto& query : pending_main_frame_redirect_chain_) {
    to_be_committed_redirect_chain_.clear();
    to_be_committed_redirect_chain_.push_back(std::move(query));
  }
}
```

Good:

```cpp
// 1. Evaluate pending query only for the pending redirect chain:
if (redirect_chain == RedirectChain::kPendingMainFrame) {
  if (pending_main_frame_query_->decision &&
      pending_main_frame_query_->decision->ShouldCancelNavigation()) {
    return pending_main_frame_query_->decision;
  }
}

// 2. Promote only while decisions remain pending; clear on resolution:
if (overall_decision) {
  pending_main_frame_redirect_chain_.clear();
} else if (decision->ShouldAllowNavigation()) {
  UpdateToBeCommittedRedirectChain();
  pending_main_frame_redirect_chain_.clear();
}

// 3. Only reset pending chain; preserve to-be-committed until navigation proceeds:
if (previous_main_frame_query_) {
  pending_main_frame_redirect_chain_.clear();
}

// 4. Clear destination container before entering the transfer loop:
void SafeBrowsingTabHelper::PolicyDecider::UpdateToBeCommittedRedirectChain() {
  to_be_committed_redirect_chain_.clear();
  for (auto& query : pending_main_frame_redirect_chain_) {
    to_be_committed_redirect_chain_.push_back(std::move(query));
  }
}
```
