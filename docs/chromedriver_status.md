# WebDriver Status

Below is a list of all WebDriver commands and their current support in ChromeDriver based on what is in the [WebDriver Specification](https://w3c.github.io/webdriver/webdriver-spec.html).

| Method | URL | Command | Status | Bug
| --- | --- | --- | --- | --- |
| POST   | /session                                                       | New Session                | Complete           |
| DELETE | /session/{session id}                                          | Delete Session             | Complete           |
| GET    | /status                                                        | Status                     | Complete           |
| GET    | /session/{session id}/timeouts                                 | Get Timeouts               | Complete           |
| POST   | /session/{session id}/timeouts                                 | Set Timeouts               | Complete           |
| POST   | /session/{session id}/url                                      | Navigate To                | Complete           |
| GET    | /session/{session id}/url                                      | Get Current URL            | Complete           |
| POST   | /session/{session id}/back                                     | Back                       | Complete           |
| POST   | /session/{session id}/forward                                  | Forward                    | Complete           |
| POST   | /session/{session id}/refresh                                  | Refresh                    | Complete           |
| GET    | /session/{session id}/title                                    | Get Title                  | Complete           |
| GET    | /session/{session id}/window                                   | Get Window Handle          | Complete           |
| DELETE | /session/{session id}/window                                   | Close Window               | Complete           |
| POST   | /session/{session id}/window                                   | Switch To Window           | Complete           |
| GET    | /session/{session id}/window/handles                           | Get Window Handles         | Complete           |
| POST   | /session/{session id}/window/new                               | New Window                 | Complete           |
| POST   | /session/{session id}/frame                                    | Switch To Frame            | Complete           |
| POST   | /session/{session id}/frame/parent                             | Switch To Parent Frame     | Complete           |
| GET    | /session/{session id}/window/rect                              | Get Window Rect            | Complete           |
| POST   | /session/{session id}/window/rect                              | Set Window Rect            | Complete           |
| POST   | /session/{session id}/window/maximize                          | Maximize Window            | Complete           |
| POST   | /session/{session id}/window/minimize                          | Minimize Window            | Complete           |
| POST   | /session/{session id}/window/fullscreen                        | Fullscreen Window          | Complete           |
| GET    | /session/{session id}/element/active                           | Get Active Element         | Complete           |
| POST   | /session/{session id}/element                                  | Find Element               | Complete           |
| POST   | /session/{session id}/elements                                 | Find Elements              | Complete           |
| POST   | /session/{session id}/element/{element id}/element             | Find Element From Element  | Complete           |
| POST   | /session/{session id}/element/{element id}/elements            | Find Elements From Element | Complete           |
| GET    | /session/{session id}/element/{element id}/selected            | Is Element Selected        | Complete           |
| GET    | /session/{session id}/element/{element id}/attribute/{name}    | Get Element Attribute      | Complete           |
| GET    | /session/{session id}/element/{element id}/property/{name}     | Get Element Property       | Complete           |
| GET    | /session/{session id}/element/{element id}/css/{property name} | Get Element CSS Value      | Complete           |
| GET    | /session/{session id}/element/{element id}/text                | Get Element Text           | Complete           |
| GET    | /session/{session id}/element/{element id}/name                | Get Element Tag Name       | Complete           |
| GET    | /session/{session id}/element/{element id}/rect                | Get Element Rect           | Complete           |
| GET    | /session/{session id}/element/{element id}/enabled             | Is Element Enabled         | Complete           |
| POST   | /session/{session id}/element/{element id}/click               | Element Click              | Partially Complete | [1996](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1996)
| POST   | /session/{session id}/element/{element id}/clear               | Element Clear              | Complete           |
| POST   | /session/{session id}/element/{element id}/value               | Element Send Keys          | Partially Complete | [1999](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1999)
| GET    | /session/{session id}/source                                   | Get Page Source            | Complete           |
| POST   | /session/{session id}/execute/sync                             | Execute Script             | Almost Complete    | [2938](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2938)
| POST   | /session/{session id}/execute/async                            | Execute Async Script       | Almost Complete    | [2938](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2938)
| GET    | /session/{session id}/cookie                                   | Get All Cookies            | Complete           |
| GET    | /session/{session id}/cookie/{name}                            | Get Named Cookie           | Complete           |
| POST   | /session/{session id}/cookie                                   | Add Cookie                 | Complete           |
| DELETE | /session/{session id}/cookie/{name}                            | Delete Cookie              | Complete           |
| DELETE | /session/{session id)/cookie                                   | Delete All Cookies         | Complete           |
| POST   | /session/{session id}/actions                                  | Perform Actions            | Complete           |
| DELETE | /session/{session id}/actions                                  | Release Actions            | Complete           |
| POST   | /session/{session id}/alert/dismiss                            | Dismiss Alert              | Complete           |
| POST   | /session/{session id}/alert/accept                             | Accept Alert               | Complete           |
| GET    | /session/{session id}/alert/text                               | Get Alert Text             | Complete           |
| POST   | /session/{session id}/alert/text                               | Send Alert Text            | Complete           |
| GET    | /session/{session id}/screenshot                               | Take Screenshot            | Complete           |
| GET    | /session/{session id}/element/{element id}/screenshot          | Take Element Screenshot    | Complete           |
