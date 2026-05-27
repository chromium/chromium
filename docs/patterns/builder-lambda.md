# The Builder Lambda Pattern

The builder lambda pattern allows you to do multi-stage construction for objects
that can't be copy-assigned without putting temporaries needed during that
construction at function scope.

Suppose you have a class `C` with some deps, which you construct like this:

    C foo;
    {
      A a;
      a.set_property(value);
      foo = MakeC(a);
    }

This requires that `C` has an "empty state" and is mutable. If you want to make
`C` immutable, that won't work, but you can use the builder lambda pattern like
this:

    C foo = [&]() -> auto {
      A a;
      a.set_property(value);
      return MakeC(a);
    }();

The anonymous lambda provides a block scope to constrain the temporary variable,
and having the lambda default capture-by-reference provides the same ability to
reference enclosing variables as a normal lexical block. Returning the result of
`MakeC()` directly into `foo` allows avoiding mutating `C`.

Note that normally, default capture by reference is
[discouraged](https://google.github.io/styleguide/cppguide.html#Lambda_expressions)
because it can create difficult lifetime problems. In this case, because the
lambda can never escape the current scope and obviously is shorter-lived than
any of the captured variables, default capture by reference is fine.
