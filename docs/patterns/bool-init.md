# The Bool Init Pattern

The `bool Init()` pattern allows a class to have initialization behavior which
can fail. Since Chromium C++ doesn't allow exceptions, ordinarily constructors
aren't allowed to fail except by crashing the entire program.

In practice, this pattern looks like this:

    class C {
     public:
      C();

      // nodiscard is best practice here, to ensure callers don't ignore
      // the failure value. It is otherwise VERY easy to accidentally ignore
      // an Init failure.
      [[nodiscard]] bool Init();
    };

and then client classes need to do something like this:

    auto c = std::make_unique<C>(...);
    if (!c->Init())
      return WELL_THAT_DIDNT_GO_SO_WELL;

## When To Use This Pattern

Probably don't. The factory pattern or unconditional initialization are
alternatives that don't require client classes to remember to call `Init()`
every time they use your class.

This pattern is often used internally as part of a class, but having a public
`Init` method that clients of your class are expected to call is error-prone -
it is too easy for client classes to forget to call it, and detecting that error
requires runtime checking inside your class. As such, you should not add a
public `Init` method. It is also important for *subclass* clients to remember to
call `Init`, which adds yet another source of error.

However, this pattern is sometimes used internally as part of the factory
pattern, in which case the factory often looks like this:

    // static
    std::unique_ptr<C> C::Make(...) {
      auto c = std::make_unique<C>(...);
      if (!c->Init())
        c.reset();
      return c;
    }

That particular use, where there is a single `Init` call site and clients cannot
otherwise acquire an uninitialized instance of `C`, is much safer but the risks
involved in subclassing `C` still apply.

## Alternatives / See also:

* The [builder](builder-and-parameter-bundle.md) pattern
* Unconditional initialization (arrange your object so that initializing it
  cannot fail)
* RAII in general
