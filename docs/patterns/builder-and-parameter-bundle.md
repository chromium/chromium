# The Builder & Parameter Bundle Patterns

The builder and parameter bundle patterns allow for giving a class a lot of
configuration options at compile time, while leaving the runtime API small and
the runtime constraints clear.

Suppose that you are implementing a new class C, which has a name, a color, a
size, and a URL associated with it, along with a handful of boolean parameters,
each of which default to false. All of these values should be set before the C
is actually used for anything. That class might look like this:

    class C {
     public:
      // Invariant: none of the members can change after you call Init().
      void Init();

     private:
      const std::string name_;
      const gfx::Color color_;
      const gfx::Size size_;
      const GURL url_;

      const bool is_cool_ = false;
      const bool is_nice_ = false;
      const bool is_friendly = false;
    };

There are three ways you might choose to make this class configurable:

1. Have the constructor take initial values for all of these arguments, or even
   have several constructors which take different subsets of the arguments.
2. Have the constructor take no arguments, and instead expose setters for all of
   these values, and enforce the invariant of DoStuff() some other way.
3. Have the constructor take values for all the arguments with no sensible
   default, and expose setters for the values that do have sensible defaults.

Most classes end up doing one of these, and have an API that looks roughly like
this:

    class C {
     public:
      C(std::string name, gfx::Color color, gfx::Size size, GURL url);

      void Init();

      void set_is_cool(bool is_cool) { is_cool_ = is_cool; }
      void set_is_nice(bool is_nice) { is_nice_ = is_nice; }
      void set_is_friendly(bool is_friendly) { is_friendly_ = is_friendly; }

     private:
      ...
      // Note that these now have to be non-const, even though they are
      // logically const, because the setters are exposed to mutate them before
      // use.
      bool is_cool_;
      bool is_nice_;
      bool is_friendly_;
    };

Your class is forced into either:

* Exposing every configuration option as a constructor argument, or
* Allowing some things that are configurable only at construction to be set
  any time after construction, with errors caught at runtime

The builder pattern (and the parameter bundle pattern, a simpler relative) offer
a way out of this bind.

## Parameter Bundles

A parameter bundle is an object that encapsulates all the parameters to another
method, often to a constructor. For example, we might structure our class C like
this:

    class C {
     public:
      struct Params {
        std::string name;
        // ...
      };

      C(Params params);

     private:
      const std::string name_;
      // ...
    };

As long as the params struct is well-structured this may be all you need, and it
can often significantly simplify constructing complicated classes if callers are
allowed to fill in only those parts of the param bundle that they care about.

However, there's a problem: constructors are not allowed to fail in C++. That
implies that it has to be impossible to *make* an invalid `C::Params`, because
if we did pass such an invalid parameter bundle into `C::C()`, the constructor
would have no way to signal failure. We really want to ensure that we can only
get an instance of `C` that is configured in a valid way.

We can do that by adding a factory function to C:

    class C {
     public:
      unique_ptr<C> Make(Params params);

     private:
      // Since this is private, we can assume Params was validated by the
      // factory function first.
      C(Params params);
    };

But in the process, we've cluttered the public API of C up with a lot of the
details about how one manufactures an instance of C. What if we could avoid
doing that? Enter...

## The Builder Pattern

The key idea here is that instead of defining a `C::Params` class, we define a
`C::Builder` class, which knows how to configure and then construct a new
instance of `C`. That might look like this:

    class C {
     public:
      class Builder;    // defined elsewhere

     private:
      friend class Builder;
      C(Builder builder);    // only called from C::Builder
    };

and now our `Builder` class has an interface like this:

    class Builder {
     public:
      Builder();
      void set_name(std::string name);
      void set_color(gfx::Color color):
      // ...

      std::unique_ptr<C> Build();
    }

To an extent, this is just a syntactic variation of the parameter bundle
pattern, but it does allow for one really nice convenience technique. If,
instead of returning void, we have our setters return a reference to the Builder
object itself:

    class Builder {
     public:
      Builder();
      Builder& set_name(std::string name);
      Builder& set_color(gfx::Color color);
      // ...

      std::unique_ptr<C> Build();
    }

then instead of writing this:

    C::Builder builder;
    builder.set_name(name);
    builder.set_color(color);
    auto c = builder.Build();

we can write this:

    auto c = C::Builder().set_name(name)
                         .set_color(color)
                         .Build();

which lets us avoid naming the temporary builder object.

## Parameter pattern only for optional parameters

It's also possible to use a version of the parameter/builder pattern only for
optional arguments. In this case we make use of `Params&` to be able to chain
calls like `Params().set_foo().set_bar()`, but we may not have a Build() method.

In the first example, this would be:

    class C {
     public:
      class Params {
       public:
        Params();

        Params& set_is_cool(bool is_cool) { is_cool_ = is_cool; return *this; }
        ...

        bool is_cool() const { return is_cool_; }
        ...

       private:
        bool is_cool_ = false;
        bool is_nice_ = false;
        bool is_friendly = false;
      };

      // Populates all fields. Optional fields take their values from `params`.
      C(std::string name, gfx::Color color, gfx::Size size, GURL url,
        Params params = Params());

     private:
      const std::string name_;
      const gfx::Color color_;
      const gfx::Size size_;
      const GURL url_;

      const bool is_cool_;
      const bool is_nice_;
      const bool is_friendly;
    };

Doing so lets us construct an object without awareness of the optional `Params`
argument:

    C("foo", ...)

While making `C` objects with optional parameters readable and without
temporary variables:

    C("foo", ..., Params().set_is_cool(true));

## Use This Pattern When

* Your class has more than half a dozen or so configuration options, most of
  which should not be changed once the object is "in use"
* Your class has invalid sets of configuration options that should be prohibited
* Your class has complicated configuration options of any sort that want runtime
  checking (places you might have previously used the [bool Init()
  pattern](bool-init.md))

## Alternatives / See Also

* Regular constructor parameters
* Instance variables with public setters

## Examples in Chromium

* [DialogModel::Builder](https://source.chromium.org/chromium/chromium/src/+/main:ui/base/models/dialog_model.h;drc=72186360334350c90b9b94515b3c82bea25e27ff;l=88)
* [ChildThreadImpl::Builder](https://source.chromium.org/chromium/chromium/src/+/main:content/child/child_thread_impl.h;l=273?q=Builder%20file:%5C.h$%20-file:%5Ethird_party%2F&ss=chromium%2Fchromium%2Fsrc)
* [TestingProfile::Builder](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/testing_profile.h;l=97?q=Builder%20file:%5C.h$%20-file:%5Ethird_party%2F&ss=chromium%2Fchromium%2Fsrc)
