---
name: ios/assert-specific-values-over-non-empty-in-tests
description: When verifying test data or collection contents, assert exact values and container sizes with EXPECT_EQ or ASSERT_EQ rather than loose relational bounds or non-empty checks
rubric: testing
severity: should_fix
---

**Rule.** In unit tests verifying state mutations, preference updates, data
retrieval, or collection contents, assert specific expected values, object
identities, or exact container sizes (`EXPECT_EQ`, `ASSERT_EQ`) rather than
loose non-null (`EXPECT_NE`), non-empty, or relational bounds (`EXPECT_GE`,
`EXPECT_GT`, `EXPECT_LE`, `ASSERT_GE`, `ASSERT_GT`, or `SizeIs(Ge(...))`). When
verifying collections, containers, or child node lists whose expected count is
deterministic and known, enforce exact counts with equality assertions rather
than relational checks that allow unexpected extra items or duplicates (reserve
inequality assertions strictly for open-ended or non-deterministic results).
Additionally, when verifying collections by inspecting individual elements or
keys, or when guarding fixture subscripts, assert the exact container count
(`[dict count]`, `container.size()`) rather than omitting size checks or
checking loose lower bounds.

**Why.** Merely checking that a value is non-null, non-empty, non-default, or
satisfies a loose relational inequality—or inspecting individual collection
elements without asserting total size—only confirms that an operation executed
or that expected items exist, silently masking bugs where stale, duplicate,
unexpected extra entries, or phantom child nodes remain. Asserting exact values
and exact container sizes validates functional correctness, enforces complete
state correctness, and ensures unintended surplus output causes tests to fail
immediately.

## Examples

Bad:

```cpp
// Relational bounds mask unexpected extra elements or duplicate child nodes.
ASSERT_GE(root_node.children_nodes_size(), 1);
EXPECT_GE(menu.children.count, 3u);

// Checking lower bounds guards indexing but allows unexpected extra items.
ASSERT_GE(items->size(), 1u);

// Loose non-null check or inspecting single keys fails to verify complete state.
EXPECT_NE(base::Time(), prefs_->GetTime(kPromoActiveDay));
EXPECT_NSEQ(@"value1", GetCrashKeys()[@"crash_key_1"]);
```

Good:

```cpp
// Exact equality fails immediately if unintended duplicate items or nodes exist.
ASSERT_EQ(root_node.children_nodes_size(), 1);
EXPECT_EQ(menu.children.count, 3u);

// Assert exact container size predetermined by fixture before subscripting.
ASSERT_EQ(items->size(), 1u);

// Assert specific expected value and exact total dictionary key count.
EXPECT_EQ(expected_time, prefs_->GetTime(kPromoActiveDay));
EXPECT_NSEQ(@"value1", GetCrashKeys()[@"crash_key_1"]);
EXPECT_EQ(1u, [GetCrashKeys() count]);
```
