# **Chromium Documentation Guidelines**

Chromium's code base is large. Very large. Like most large places, it can be hard to find your way around if you don't already know the way. It also changes a lot. Lots of people work on Chromium and refactoring, componentization, addition
or removal of layers, etc. means that knowledge one has can quickly get out of date.

As a result, it can be hard to figure out how things are supposed to fit together. Documentation on [dev.chromium.org](http://dev.chromium.org/developers) can be hard to navigate and harder to keep up to date. The [starter guide](https://sites.google.com/a/chromium.org/dev/developers/how-tos/getting-around-the-chrome-source-code) is helpful, but stops at a very high-level overview. Ultimately [codesearch.chromium.org](http://codesearch.chromium.org) is the way most people explore.

This guide attempts to lay out some practices that will make it easier for newcomers to find their way around Chromium's code and for old hands to keep up with a constantly evolving code base. It works from the principle that all documentation is wrong and out of date, but in-code documentation is less so and valuable.

## Guidelines

In-code documentation can be categorized into three tiers:

*   **Module / component documentation**: This is documentation that tends to be found in README.md files at the root level directory of a component. The purpose of this type of documentation is to explain what a given set of related classes are for, roughly how they are structured and *what* the component is expected to be used for.
*   **Interface / class documentation**: This is documentation most often found in header files near class definitions. This may describe what a class or small set of classes are for and *how* they are intended to be used. It may also provide examples of usage of the class where this isn't obvious.
*   **Implementation documentation**: This type of documentation is found next to implementations. In C++, this will tend to be embedded in .cc files. It is meant to help a reader understand *how the code works* or explain an implementation path that might not be obvious from the code itself.

Let's dig in a bit to how to use these types of documentation in Chromium.

### Module / component documentation

Ok, so you're working on something big and important. Maybe it's a chunk of code that merits living in its own component in [src/components/](https://codesearch.chromium.org/chromium/src/components/) or even directly under [src/](https://codesearch.chromium.org/chromium/src/) itself. One day someone's going to come along and wonder what it's for and they'll discover that awesome-directory-name (breakpad, courgette, mojo, rutabaga, quick: which one of those was made up?) doesn't by itself shed a lot of light on what the code does.

***Any self contained unit of code that merits a container directory should have a README.md file that describes what that component is, how it is expected to be used and provides rough outline of the code's structure.***

The README.md file should be located at the root of the component directory and should be in [markdown format](/styleguide/markdown/markdown.md).

The first thing in the README.md file should be a description of the component in sufficient detail that someone unfamiliar with the code will understand why it exists. It should answer the questions "what does this component do?" and "is there any in particular I should know about it?". Some larger components (e.g. v8, mojo, etc.) may have additional up-to-date documentation on [dev.chromium.org](http://dev.chromium.org) or elsewhere makes sense too.

The second thing in the README.md file should be an outline of how it is expected to be used. For components of small to moderate size (e.g. under [src/components/](https://codesearch.chromium.org/chromium/src/components/)), this can mean what are the main classes to care about. For larger components this can describe how to go about using the component, possibly in the form of links to external documentation (e.g.: [src/v8/](https://codesearch.chromium.org/chromium/src/v8/README.md)).

The third thing should be a lay of the land of the component, sufficient breadcrumbs for a newcomer to be able to orient themselves. This usually means at least an explanation of the subdirectories of the component and possibly a description of the major interfaces a consumer of the component will need to care about. Again for larger components this can live in external kept-up-to-date documentation where needed.

The fourth thing should be historical links to design docs for the component, including early ones. These are super handy for people looking to understand why the component exists in its current form.

A great example of a README.md file is [src/native_client/README.md](https://codesearch.chromium.org/chromium/src/native_client/README.md). There are many more examples that are waiting to be made great.

### Interface / class documentation

Class and function declarations in header files describe to the world how a piece of code should be used. The public interface of a class or a header file containing function declarations describe the API to the code but don't necessarily say much about how it is expected to be used. As such:

***Non-trivial classes or sets of function declarations in header files should include comments with a brief description of the purpose of the class or set of functions as well as examples of expected usage.***

(Good) examples:

https://codesearch.chromium.org/chromium/src/base/callback_list.h

https://codesearch.chromium.org/chromium/src/base/feature_list.h

### Implementation documentation

Code should be as self documenting as possible, but sometimes that isn't enough. Maybe there's a tricky bit of implementation that isn't obvious. Maybe there's a clever algorithm that has a good reason it needs to be clever. A good rule of thumb is that if a code reviewer had a clarifying question about something during review and the code couldn't be simplified to answer the question then a comment is probably appropriate. Don't go nuts here though, often if code is so complicated that it needs lots of comments, then it could probably be simplified first.

## Language

Be concise.

Use correct grammar as much as possible.

Avoid fancy or confusing language.

Don't assume that readers know everything you currently know.

## Working with existing code

If you got this far and have some experience with Chromium's code, you'll have figured out that these guidelines are aspirational more than what the world looks like today. So what do we do when working with existing code.

First off: ***[Documentation changes can be TBRed](https://chromium.googlesource.com/chromium/src/+/main/docs/code_reviews.md#documentation-updates)***. Even in-code changes. If you have discovered something that isn't documented, have figured out how it works and would like to pay it forward, feel free to write something down and check it in.

At the component level, if you are the owner of a component that isn't documented, please add a README.md with content as per the above.

If you don't own a component and figure out how it works, consider adding some notes to the component's README.md file. You don't need to be an expert to share what you do know with those who come afterwards.

The same applies to class level comments and implementation comments. If you find yourself modifying code and realizing that it is… under-documented.. feel free to add some notes nearby.

In general, use your judgment. Changes will NOT be blocked on requiring people to add missing documentation, but adding missing docs is a great way to make our code base healthier.

## FAQ

#### Are these hard rules?

[Well, no](https://www.youtube.com/watch?v=jl0hMfqNQ-g). This said, following these guidelines will make our code base healthier, more consistent and much easier to understand. Over time improving in-code documentation will make the whole team faster. But if a small fix is genuinely needed (it almost always isn't), then don't block on writing detailed documentation. As always: use your judgment.

#### I'm not an expert on this undocumented code, should I still add documentation?

Yes! Partial documentation is much better than no documentation.

#### I hate writing documentation, it will slow me down!

Chromium is big enough and complicated enough that a newcomer has to read a lot of code to figure out how things fit together. Documentation provides breadcrumbs to speed up understanding, which over time will make the whole team work more quickly. Short term execution speed is far less important than long term team velocity.
