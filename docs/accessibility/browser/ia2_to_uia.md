# Runtime two way IAccessible2 to UI Automation elements look up via unique id
Assistive technologies (ATs) who currently rely on IAccessible2 (IA2) that want
to take advantage of the UI Automation (UIA) features at runtime can convert an
IA2 element to an UIA element via a unique id and directly access UIA's API.
This enables ATs who want to gradually transition from IA2 to UIA to experiment
with individual UIA elements at runtime without switching entirely to UIA.


To look up an UIA element through a unique id, an AT can utilize
`IUIAutomationItemContainerPattern::FindItemByProperty()` with the custom UIA
unique id property and the element's unique id as parameters.
To look up an IA2 element from an UIA element, an AT can simply
utilize IUIAutomationLegacyIAccessiblePattern::GetIAccessible() and
then query for IAccessible2 interface. The unique id is not needed to
look up the IA2 element from UIA element.

## Convert an IA2 element to UIA element via unique id
An IA2 element can be converted to an UIA element at runtime via a unique id
that is shared between the two APIs.

*Note: For the purpose of brevity and clarity, the code snippets below do not
include clean-up of COM references neither does it have error handling.*

~~~c++
  #include <uiautomation.h>
  #include <uiautomationclient.h>

  // Consider the following HTML:
  // <html>
  //  <button>button</button>
  // </html>

  // Register custom UIA property for retrieving the unique id of IA2 object.
  // {cc7eeb32-4b62-4f4c-aff6-1c2e5752ad8e}
  GUID UiaPropertyUniqueIdGuid = {
      0xcc7eeb32,
      0x4b62,
      0x4f4c,
      {0xaf, 0xf6, 0x1c, 0x2e, 0x57, 0x52, 0xad, 0x8e}};

  // Create the registrar object and get the IUIAutomationRegistrar
  // interface pointer.
  IUIAutomationRegistrar* registrar;
  CoCreateInstance(CLSID_CUIAutomationRegistrar, nullptr, CLSCTX_INPROC_SERVER,
                   IID_PPV_ARGS(&registrar));

  // Custom UIA property id used to retrieve the unique id between UIA/IA2.
  PROPERTYID uia_unique_id_property_id;

  // Register the custom UIA property that represents the unique id of an UIA
  // element which also matches its corresponding IA2 element's unique id.
  // Custom property registration only needs to be done once per process
  // lifetime.
  UIAutomationPropertyInfo unique_id_property_info = {
      UiaPropertyUniqueIdGuid, L"UniqueId", UIAutomationType_String};
  registrar->RegisterProperty(&unique_id_property_info,
                              &uia_unique_id_property_id);

  // Assume we are given the IAccessible2 element for button, and we want to
  // retrieve its corresponding UIA element for the final result.
  IAccessible2* button_ia2; /* Initialized */

  // Retrieve button IA2 element's unique id, which will be used to look up the
  // corresponding UIA element later.
  LONG unique_id_long;
  button_ia2->get_uniqueID(&unique_id_long);

  // Assume we are given the Window Handle hwnd for the root.
  UIA_HWND hwnd; /* Initialized */

  // Instantiating an IUIAutomation object.
  IUIAutomation* ui_automation;
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_PPV_ARGS(&ui_automation));

  // Retrieve the root element from the window handle.
  IUIAutomationElement* root_element;
  ui_automation->ElementFromHandle(hwnd, &root_element);

  // Retrieve the ItemContainerPattern of the root element.
  IUIAutomationItemContainerPattern* item_container_pattern;
  root_element->GetCurrentPatternAs(UIA_ItemContainerPatternId,
                                    IID_PPV_ARGS(&item_container_pattern));

  // We also need to convert the retrieved IA2 element unique id from long to
  // VARIANT.VT_BSTR to be consumed by UIA. For demo purpose, I utilize
  // std::string here as an intermediary step to convert to VARIANT.VT_BSTR.
  std::wstring unique_id_str = std::to_wstring(unique_id_long);

  VARIANT unique_id_variant;
  unique_id_variant.vt = VT_BSTR;
  unique_id_variant.bstrVal = SysAllocString(unique_id_str.c_str());

  // Retrieving the corresponding UIAutomation element from the unique id of IA2
  // object.
  IUIAutomationElement* button_uia;
  item_container_pattern->FindItemByProperty(nullptr, uia_unique_id_property_id,
                                             unique_id_variant,
                                             &button_uia /* final result */);
~~~

## Convert an UIA element to IA2 element.
Converting an UIA element to an IA2 element is a lot more straightforward and
does not require the shared unique id. Consider the same example above.
~~~c++
  #include <uiautomationclient.h>

  // Assume we are given the UIAutomation element for button, and we want to
  // retrieve its corresponding IAccessible2 element for the final result.
  IUIAutomationElement* button_uia; /* Initialized */

  // Retrieve the LegacyIAccessiblePattern of the button UIA element.
  IUIAutomationLegacyIAccessiblePattern* legacy_iaccessible_pattern;
  button_uia->GetCurrentPatternAs(
      UIA_LegacyIAccessiblePatternId,
      IID_PPV_ARGS(&legacy_iaccessible_pattern));

  // Retrieve the IAccessible element from button UIA element.
  // Note: According to UIA doc, GetIAccessible returns NULL if a client
  // attempts to retrieve the IAccessible interface for an element originally
  // supported by a proxy object from OLEACC.dll, or by the UIA-to-MSAA Bridge.
  // https://docs.microsoft.com/en-us/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomationlegacyiaccessiblepattern-getiaccessible
  IAccessible* button_iaccessible;
  legacy_iaccessible_pattern->GetIAccessible(&button_iaccessible);

  // Use QueryService to retrieve button's IAccessible2 element from IAccessible
  // element.
  IServiceProvider* service_provider;
  IAccessible2* button_ia2;
  if (SUCCEEDED(button_iaccessible->QueryInterface(
          IID_PPV_ARGS(&service_provider)))) {
    service_provider->QueryService(
        IID_IAccessible, IID_PPV_ARGS(&button_ia2 /* final result */));
  }
~~~

## Docs & References:
[Custom UIA Property and Pattern registration in Chromium](https://chromium.googlesource.com/chromium/src/+/main/ui/accessibility/platform/uia_registrar_win.h)

[UI Automation IItemContainerPattern. It is used to look up IAccessible2 element
via a unique id](https://docs.microsoft.com/en-us/windows/win32/api/uiautomationclient/nn-uiautomationclient-iuiautomationitemcontainerpattern)

[UI Automation Client](https://docs.microsoft.com/en-us/windows/win32/api/uiautomationclient/)
