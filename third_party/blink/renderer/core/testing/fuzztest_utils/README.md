# DOM Fuzzing Utilities

This directory contains utilities for creating FuzzTest-based DOM fuzzers in Blink.

## Core Components

### Domain Functions
These functions can be used within the DomScenario framework described below
or as standalone FuzzTest domains for custom test scenarios. Additional domain
functions, as well as support for other specifications, are encouraged.

- **`html_domains.h/cc`** - HTML tag and attribute generators
- **`mathml_domains.h/cc`** - MathML tag and attribute generators
- **`svg_domains.h/cc`** - SVG tag and attribute generators
- **`css_domains.h/cc`** - CSS property and value generators
- **`aria_domains.h/cc`** - ARIA attribute and value generators
- **`common_domains.h/cc`** - Shared utilities for domain composition

### DomScenario Framework
- **`NodeState`** - Represents the mutable state of a DOM node: parent index,
  attributes, styles, and text content.
- **`NodeSpecification`** - Represents a single DOM node with its tag, initial
  state, and modified state. Each node has a 1:1 mapping between initial and
  modified states.
- **`DomScenario`** - Represents a complete test case containing a root element
  tag, a vector of `NodeSpecification` objects, and an optional stylesheet
  injected as a `<style>` element in the document head.
- **`DomScenarioDomainSpecification`** - Interface that defines domain-specific
  fuzzing parameters. Used by `AnyDomScenarioForSpec()` to generate appropriate
  `NodeSpecification` objects. Implementations must specify:
  - `AnyTag()` - the pool of possible tags
  - `AnyAttributeNameValuePair()` - the pool of possible attributes and values
  - `AnyStyles()` - the pool of possible inline styles
  - `AnyText()` - the pool of possible text content
  - `GetMaxDomNodes()`, `GetMaxAttributesPerNode()` - tree size limits
  - `GetRootElementTag()` - root element type
  - `AnyStylesheet()` (optional) - the pool of possible stylesheets (defaults
    to empty string)
  - `GetPredefinedNodes()` (optional) - provides a fixed initial DOM structure
    instead of generating random nodes. Modifications are still fuzzed.
  - `UseShadowDOM()` (optional) - returns true to enable shadow DOM fuzzing,
    which wraps nodes in shadow hosts with optional slot projection (defaults
    to false)
- **`AnyDomScenarioForSpec()`** - Generates FuzzTest domains from specifications
- **`DomScenarioRunner`** - Base class that executes `DomScenario` test cases
  by creating initial DOM, applying modifications, and calling
  `UpdateStyleAndLayoutTree()` after each phase. Subclasses can override
  `CreateInitialDOM()` and `ApplyModifications()` to add custom behavior (e.g.,
  dumping accessibility trees). Includes detailed logging (enabled with
  `--enable-dom-fuzzer-logging`) and `DomScenario::ToString()` for debugging.

## Usage Example

See `html_dom_fuzztest.cc` for a complete example of how to use this framework
to create HTML DOM fuzzers with CSS styles, attributes, and text content.
