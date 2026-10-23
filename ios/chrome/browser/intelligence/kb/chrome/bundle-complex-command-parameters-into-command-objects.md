---
name: ios/bundle-complex-command-parameters-into-command-objects
description: When a UI command takes more than three parameters or has multiple variants, encapsulate them into a Command object payload.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Characteristics of a UI Command
    quote: When a command requires more than three parameters, or has multiple variants taking different combinations of model-layer parameters, bundle those parameters into a command object and pass that object as the sole parameter.
---

**Rule.** When a command takes more than three parameters or has multiple
signature variants, bundle the arguments into an immutable
`<Verb><Feature>Command` object passed as the sole argument.

**Why.** Bundling parameters into command objects prevents signature churn at
call sites as features evolve and keeps protocol declarations readable. See
`ios/architecture/commands.md#characteristics-of-a-ui-command`.

## Examples

Bad:

```cpp
- (void)showQRCodeWithURL:(const GURL&)URL title:(NSString*)title subtitle:(NSString*)subtitle icon:(UIImage*)icon sender:(id)sender;
```

Good:

```cpp
- (void)showQRCode:(GenerateQRCodeCommand*)command;
```
