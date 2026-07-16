# Prefer enums over bools for parameters

When you want to add a bool parameter to a function, consider giving the
function a two-value enum parameter instead.

Compare these call sites:

```
my_function1(false, true);
```

```
my_function1(Sandbox::Disable, RecordStats::Yes);
```

Not only is the latter easier to read, it's also impossible to accidentally
swap the order of parameters.

It's not much more code to declare either:

```
enum class Sandbox { Disable, Enable };
enum class RecordStats { Disable, Enable };
void my_function1(Sandbox enable_sandbox, RecordStats record_stats);
```

If `my_function1()` is a member function and the enum is used only for
that one function, you can declare the enum class right above the member
function, so that call sites become
`instance->set_flag(Class::Enum::Yes)`.
