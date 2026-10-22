---
name: ios/bundle-complex-command-parameters-into-command-objects
rubric: design
severity: should_fix
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
