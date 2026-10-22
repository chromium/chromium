---
name: ios/prefer-unique-ptr-over-scoped-refptr-for-single-ownership
rubric: design
severity: should_fix
---

**Rule.** Use `std::unique_ptr<T>` rather than `scoped_refptr<T>` for
heap-allocated member variables, services, and state objects whose lifetime is
exclusively owned and managed by a single enclosing container or class (or
prefer an inline value/`std::optional<T>` member when heap allocation is
unnecessary; see `ios/prefer-optional-or-value-over-unique-ptr`). Reserve
`scoped_refptr<T>` strictly for objects that require shared ownership across
multiple owners or sequences.

**Why.** Holding exclusively owned objects via `scoped_refptr<T>` introduces
unnecessary refcounting overhead, obscures single-ownership architecture, and
forces classes to inherit from `base::RefCounted`. Using `std::unique_ptr<T>`
guarantees clear lifetime boundaries and prevents unintentional reference
retention or cyclic leaks.

## Examples

Bad:

```cpp
class ApplicationContextImpl {
 private:
  scoped_refptr<ModelBrokerState> model_broker_state_;
};

void ApplicationContextImpl::Init() {
  model_broker_state_ = base::MakeRefCounted<ModelBrokerState>();
}
```

Good:

```cpp
class ApplicationContextImpl {
 private:
  std::unique_ptr<ModelBrokerState> model_broker_state_;
};

void ApplicationContextImpl::Init() {
  model_broker_state_ = std::make_unique<ModelBrokerState>();
}
```
