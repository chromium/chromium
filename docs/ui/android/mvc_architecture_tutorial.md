# So, you want to do MVC...

### Overview
A full explanation of the MVC framework can be found [here](mvc_overview.md). This document is intended to go over the logistics of the most basic implementation of the framework in Chrome’s codebase.

For this example, we’ll be implementing a simple progress bar; a rectangle that changes length based on the loading state of the underlying webpage.

#### Additional Resources
* [Simple MVC lists](mvc_simple_list_tutorial.md)
* [Testing MVC primer doc](mvc_testing.md)

#### File Structure
The file structure of our component will be the following:
* ./org/chromium/chrome/browser/simple_progress/
  * [`SimpleProgressCoordinator.java`](#SimpleProgressCoordinator)
  * [`SimpleProgressMediator.java`](#SimpleProgressMediator)
  * [`SimpleProgressViewBinder.java`](#SimpleProgressViewBinder)
  * [`SimpleProgressProperties.java`](#SimpleProgressProperties)
  * `SimpleProgressView.java` (assuming it requires interesting logic)

#### SimpleProgressCoordinator
The class responsible for setting up the component. This should be the only public class in the component's package and is the only class with direct access to the mediator.

```java
public class SimpleProgressCoordinator {

   private final SimpleProgressMediator mMediator;
   private final View mView;

   public SimpleProgressCoordinator (Tab tabProgressBarIsFor, Context context) {

       PropertyModel model = new PropertyModel.Builder(SimpleProgressProperties.ALL_KEYS)
               .with(SimpleProgressProperties.PROGRESS_FRACTION, 0f)
               .with(SimpleProgressProperties.FOREGROUND_COLOR, Color.RED)
               .build();

       mView = LayoutInflater.from(context).inflate(R.layout.my_simple_progress_bar);

       PropertyModelChangeProcessor.create(model, mView, SimpleProgressViewBinder::bind);

       mMediator = new SimpleProgressMediator(model, tabProgressBarIsFor);
   }

   public void destroy() {
       mMediator.destroy();
   }

   public View getView() {
       return mView;
   }
}
```
Note that there are several ways to acquire the view. If this MVC component owns its own layout file, then it should inflate the view, as shown above. #getView() allows the parent component's coordinator to add this component's view to the hierarchy. However, if the desired view for this MVC component is part of some other parent component's layout file, then the parent component should be responsible for calling findViewById() and passing the right view into this coordinator. See [this email thread](http://g/clank-frontend/u8x2PBa5EfI) for related discussion.

#### SimpleProgressMediator
The class that handles all of the signals coming from the outside world. External classes should never interact with this class directly.

```java
class SimpleProgressMediator extends EmptyTabObserver {

   private final PropertyModel mModel;
   private final Tab mObservedTab;

   public SimpleProgressMediator(PropertyModel model, Tab tabProgressBarIsFor) {
       mModel = model;
       mObservedTab = tabProgressBarIsFor;
       mObservedTab.addObserver(this);
   }

   @Override
   public void onLoadProgressChanged(Tab tab, int progress) {
       mModel.set(SimpleProgressProperties.PROGRESS_FRACTION, progress / 100f);
   }

   @Override
   public void onDidChangeThemeColor(Tab tab, int color) {
       mModel.set(SimpleProgressProperties.FOREGROUND_COLOR, color);
   }

   void destroy() {
       // Be sure to clean up anything that needs to be (in this case, detach the tab
       // observer).
       mObservedTab.removeObserver(this);
   }
}
```

#### SimpleProgressViewBinder
The class responsible for applying a model to a specific view. In general there is a 1:1 relationship between a type of view and its binder. Multiple binders can know how to take a single type of model and apply it to a view. The binder's method should be stateless; this is implied by the 'static' identifier.

```java
class SimpleProgressViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (SimpleProgressProperties.PROGRESS_FRACTION == propertyKey) {

            // Apply width modification to 'view' here.

        } else if (SimpleProgressProperties.FOREGROUND_COLOR == propertyKey) {

            // Apply color modification to 'view' here.

        }
    }
}
```

#### SimpleProgressProperties
These are properties associated with the view the model will be applied to.

```java
class SimpleProgressProperties {

    public static final WritableFloatPropertyKey PROGRESS_FRACTION =
            new WritableFloatPropertyKey();

    public static final WritableIntPropertyKey FOREGROUND_COLOR =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {PROGRESS_FRACTION, FOREGROUND_COLOR};
}
```
