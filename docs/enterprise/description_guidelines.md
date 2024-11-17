# Policy Description Guidelines

## Summary

The policy description provides details of a policy. It's defined in the `desc`
field in each yaml files and will be used to generate
policy doc site
[chromeenterprise.google/policies/](https://chromeenterprise.google/policies/),
ADM/AMDX templates or some other 3rd party management tools UI.


## Structure

In general, the policy descriptions should follow the structure below.
```
<Overview of the policy>

<Background>

<One or more bullet points for setup>

<Any other information>
```


### Overview
Using one or two sentences to explain the main function that the policy
controls.

### Background
It is usually worth a few sentences to describe what the function actually does.
Keep in mind that the readers of the policy description are likely not experts
in this area.

### Setup
It’s important to describe different states of a policy, including the unset
state. Please also mention whether users can control any setting when the policy
is unset or set to a specific value.


##### Boolean
Boolean policies usually have 3 states: `Enabled`, `Disabled` and `not set`.

```
Setting the policy to Enabled, ...

Setting the policy to Disabled, ...

Not setting the policy, ...
```

Please use `Enabled` with capital `E` and `Disabled` with capital `D`.

In some cases, if the policy is default to `Enabled` or `Disabled`, combine the
`not set` one to it.

```
Setting the policy to Enabled or not set, ...

Setting the policy to Disabled, ..
```

If you need a long sentence to describe the policy state or need to repeat
yourself in each state, consider putting that information in the background
section above and keep the sentences here short.


#### Enum
Similar to boolean, all enum options need to be listed separately. When
referring to enum options, please mention their values and captions.

```
Setting the policy to <0 - caption of option 0>, ...

Setting the policy to <1 - caption of option 1>, ...

...
Not setting the policy, ...
```

Same as boolean, if you need a long sentence to describe the policy state
or need to repeat yourself in each state, consider putting that information in
the background section above and keep the sentences here short.


#### String/List/Dictionary
Those policies are usually complicated. In general, please describe the input
requirements, like what each key of the dictionary means or what kind of string
format can be accepted.


#### Others
Any other useful information can be provided here. Here are some common topics.

##### Exceptions
Uncommon behavior. For example, most policies override user settings by default.
If a policy configuration won’t do so, it definitely needs to be highlighted.


##### Conflicts
When a policy can have a conflict with other policies or contains two
mutual options, it’s always a good idea to talk about those edge cases. For
example, if there is an allow list and block list, what if an option is put into
both lists?


##### Expiration
Some policies are planned to be only available for a certain amount of time.
It’s very important to communicate it ahead of time. However, it’s ok not to
mention a specific date if the timeline is not decided yet.


##### Not launched
Same as expiration, a policy can be added before the feature is launched. Please
describe the default behavior now and once the feature is rolled out.


##### Sensitive policies
Malware could abuse policies to control users' devices. Some policies are
particularly dangerous so there will be additional management requirements
before applying them. For those policies, please append the following statements
at the end of the policy description according to their supported platforms.

```
On <ph name="MS_WIN_NAME">Microsoft® Windows®</ph>, this policy is only available on instances that are joined to a <ph name="MS_AD_NAME">Microsoft® Active Directory®</ph> domain, joined to <ph name="MS_AAD_NAME">Microsoft® Azure® Active Directory®</ph> or enrolled in <ph name="CHROME_BROWSER_CLOUD_MANAGEMENT_NAME">Chrome Browser Cloud Management</ph>`.

On <ph name="MAC_OS_NAME">macOS</ph>, this policy is only available on instances that are managed via MDM, joined to a domain via MCX or enrolled in <ph name="CHROME_BROWSER_CLOUD_MANAGEMENT_NAME">Chrome Browser Cloud Management</ph>.
```


#### Too many details
The policy description can’t be longer than 3500 characters. However, even
before that limit is reached, as long as it’s necessary, consider creating a
webpage for additional details and link it from the policy description.

Usually, this can be a help center article. Please contact a tech writer to
create one for you. It can also be any public documentation that can help. For
example, if the policy accepts urls as input, you can link
[this](https://support.google.com/chrome/a?p=url_blocklist_filter_format) or
[this](https://chromeenterprise.google/policies/url-patterns/) article to avoid
repeating most edge cases of URL input and only focus on the unique part.

Note that when linking the help center article, please use a p link (e.g.
`a?p=url_blocklist_filter_format`) instead of a number link (e.g.
`a/answer/9942583`). Contact a tech writer to create the p link for you.


## Format
The Policy description will be displayed in various different tools. Some of
these tools may not support any kind of formatting except splitting each
paragraph with an empty line.

```
Line A

Line B

Line C
```

Note that single line return may be used to keep the line length reasonably
short. But it will be ignored in some generated documentations. In other words,

```
Line A
Line B
Line C
```

Will be presented as
```
Line A Line B Line C
```

If complicated formatting is needed, please create a help center article to
include better expression such as graphs and tables.


## Translation
All policy descriptions and captions will be translated into multiple languages.
Avoid using phrases that are unique to English culture and hard to translate.

In addition to that, to ensure consistency in the policy descriptions, the
following is a mapping of how various product names and the like should be
referenced. All placeholders tags must be opened and closed on the same line to
avoid validation errors.

#### Chrome Product
* Chrome: `<ph name="PRODUCT_NAME">$1<ex>Google Chrome</ex></ph>`
* ChromeOS: `<ph name="PRODUCT_OS_NAME">$2<ex>Google ChromeOS</ex></ph>`
* ChromeOS Flex: `<ph name="PRODUCT_OS_FLEX_NAME">Google ChromeOS Flex</ph>`
* Chrome Browser Cloud Management: `<ph name="CHROME_BROWSER_CLOUD_MANAGEMENT_NAME">Chrome Browser Cloud Management</ph>`
* Chrome Cleanup: `<ph name="CHROME_CLEANUP_NAME">Chrome Cleanup</ph>`
* Chrome Remote Desktop: `<ph name="CHROME_REMOTE_DESKTOP_PRODUCT_NAME">Chrome Remote Desktop</ph>`
* Chrome Sync: `<ph name="CHROME_SYNC_NAME">Chrome Sync</ph>`

#### Google Product
* Google Admin console: `<ph name="GOOGLE_ADMIN_CONSOLE_PRODUCT_NAME">Google Admin console</ph>`
* Google Calendar: `<ph name="GOOGLE_CALENDAR_NAME">Google Calendar</ph>`
* Google Cast: `<ph name="PRODUCT_NAME">Google Cast</ph>`
* Google Classroom: `<ph name="GOOGLE_CLASSROOM_NAME">Google Classroom</ph>`
* Google Cloud Print: `<ph name="CLOUD_PRINT_NAME">Google Cloud Print</ph>`
* Google Drive: `<ph name="GOOGLE_DRIVE_NAME">Google Drive</ph>`
* Google Photos: `<ph name="GOOGLE_PHOTOS_PRODUCT_NAME">Google Photos</ph>`
* Google Tasks: `<ph name="GOOGLE_TASKS_NAME">Google Tasks</ph>`
* Google Update: `<ph name="GOOGLE_UPDATE_NAME">Google Update</ph>`
* Google Workspace: `<ph name="GOOGLE_WORKSPACE_PRODUCT_NAME">Google Workspace</ph>`

#### Operating System
* Android: `<ph name="ANDROID_NAME">Android</ph>`
* Fuchsia: `<ph name="FUCHSIA_OS_NAME">Fuchsia</ph>`
* iOS: `<ph name="IOS_NAME">iOS</ph>`
* Linux: `<ph name="LINUX_OS_NAME">Linux</ph>`
* macOS: `<ph name="MAC_OS_NAME">macOS</ph>`
* Windows: `<ph name="MS_WIN_NAME">Microsoft® Windows®</ph>`


#### Other Product
* Internet Explorer: `<ph name="IE_PRODUCT_NAME">Internet® Explorer®</ph>`
* Microsoft Active Directory: `<ph name="MS_AD_NAME">Microsoft® Active Directory®</ph>`
* Microsoft Azure Active Directory `<ph name="MS_AAD_NAME">Microsoft® Azure® Active Directory®</ph>`

Using placeholders means the text won’t be translated. Please update the list
above if a new placeholder is introduced.
