# UIA client process hashes

UIA client process histograms record the signed sparse histogram sample from
`static_cast<int>(base::PersistentHash(process_name))`, where `process_name` is
the lowercased process basename.

Keep this list ordered alphabetically by process name. Add newly identified UIA
client process names here alongside their hash values.

Windows records only the lowercased basename. Thus you cannot see the file path,
the publisher, or the code signature. Some process names belong to more than one
product. Where no product is clearly dominant, the client column shows
`Unidentified`.

| Process name | Known client | `base::PersistentHash` | Sparse sample |
| --- | --- | ---: | ---: |
| `360tray.exe` | Qihoo 360 security products | `1143570600` | `1143570600` |
| `asusdialagent.exe` | ASUS Dial & Control Panel | `3734002420` | `-560964876` |
| `attrib.exe` | Windows attrib command | `4157810384` | `-137156912` |
| `autocorrect.exe` | Unidentified | `1838143251` | `1838143251` |
| `autoglm.exe` | Unidentified | `2310455186` | `-1984512110` |
| `avastbrowser.exe` | Avast Secure Browser | `2428358996` | `-1866608300` |
| `avastui.exe` | Avast Antivirus | `4192374751` | `-102592545` |
| `awesun.exe` | AweRay AweSun Remote Desktop | `3232297458` | `-1062669838` |
| `baidupinyin.exe` | Baidu Pinyin IME | `2294407191` | `-2000560105` |
| `bemyeyes.exe` | Be My Eyes | `979294987` | `979294987` |
| `brave.exe` | Brave Browser | `1946420109` | `1946420109` |
| `browser.exe` | Unidentified | `1712015162` | `1712015162` |
| `calc.exe` | Windows Calculator launcher | `1314406447` | `1314406447` |
| `chrome.exe` | Google Chrome | `452333607` | `452333607` |
| `cici.exe` | ByteDance Cici | `58353144` | `58353144` |
| `clipboardmaster.exe` | Jumping Bytes ClipboardMaster | `3190348616` | `-1104618680` |
| `clipdown.exe` | ClipDown | `161696600` | `161696600` |
| `comet.exe` | Perplexity Comet | `123029657` | `123029657` |
| `ctfmon.exe` | CTF Loader | `2426234962` | `-1868732334` |
| `cyberhavensessionmonitor.exe` | Cyberhaven Lightbeam | `867957808` | `867957808` |
| `deepl.exe` | DeepL | `772432034` | `772432034` |
| `desktime.exe` | DeskTime | `3627214626` | `-667752670` |
| `dingtalk.exe` | DingTalk | `4090957852` | `-204009444` |
| `dolsrvcbar2.exe` | Dolphin CBar service | `1347269751` | `1347269751` |
| `dotnet.exe` | .NET CLI host | `2176324593` | `-2118642703` |
| `doubao.exe` | ByteDance Doubao | `2791971446` | `-1502995850` |
| `dwm.exe` | Desktop Window Manager | `68647119` | `68647119` |
| `eaio_agent.exe` | Unidentified | `4007921950` | `-287045346` |
| `eoaexperiences.exe` | Windows text cursor indicator | `2075022684` | `2075022684` |
| `excel.exe` | Microsoft Excel | `2553980610` | `-1740986686` |
| `executor.exe` | Unidentified | `272886190` | `272886190` |
| `explorer.exe` | Windows File Explorer | `721751405` | `721751405` |
| `feishu.exe` | ByteDance Feishu | `2362193036` | `-1932774260` |
| `fluentsearch.exe` | Fluent Search | `482325164` | `482325164` |
| `galaxy.exe` | Unidentified | `1049954830` | `1049954830` |
| `globalpresenter.exe` | Lenovo Voice Assistant | `3794748260` | `-500219036` |
| `google.exe` | Unidentified | `1458698666` | `1458698666` |
| `grammarly.desktop.exe` | Grammarly for Windows | `1559241653` | `1559241653` |
| `grammarly.exe` | Grammarly (legacy) | `3287231323` | `-1007735973` |
| `hal.exe` | Dolphin SuperNova | `1976295005` | `1976295005` |
| `ideashare.exe` | Huawei IdeaShare | `2063844378` | `2063844378` |
| `idm.exe` | Internet Download Manager | `2563882776` | `-1731084520` |
| `inspect.exe` | Windows SDK Inspect | `509874236` | `509874236` |
| `installer.exe` | Unidentified | `2190055868` | `-2104911428` |
| `jfw.exe` | JAWS | `1248433627` | `1248433627` |
| `jsnpmon.exe` | Jiransoft (product unidentified) | `2905155242` | `-1389812054` |
| `lark.exe` | ByteDance Lark | `1784603500` | `1784603500` |
| `logioptionsmgr.exe` | Logitech Options | `727268717` | `727268717` |
| `lpbint.exe` | DataResolve lpagnt agent | `804314869` | `804314869` |
| `lzsmartrec.exe` | Unidentified | `2144042677` | `2144042677` |
| `magic.exe` | Freedom Scientific MAGic | `2562083169` | `-1732884127` |
| `magictaskbar-ui.exe` | HONOR MagicTaskbar | `2896638579` | `-1398328717` |
| `magictext.exe` | HONOR MagicText | `1006442042` | `1006442042` |
| `magnify.exe` | Windows Magnifier | `3025551654` | `-1269415642` |
| `manictime.exe` | ManicTime | `2712665347` | `-1582301949` |
| `memory.exe` | Unidentified | `823918155` | `823918155` |
| `msedge.exe` | Microsoft Edge | `3763942626` | `-531024670` |
| `mspaint.exe` | Microsoft Paint | `1788395968` | `1788395968` |
| `mspcmanager.exe` | Microsoft PC Manager | `1469064659` | `1469064659` |
| `narrator.exe` | Windows Narrator | `2638653865` | `-1656313431` |
| `natspeak.exe` | Dragon NaturallySpeaking | `272349872` | `272349872` |
| `nortonui.exe` | Norton security products | `1884249869` | `1884249869` |
| `nutstore.windowshook.exe` | Nutstore Windows Hook | `1052318032` | `1052318032` |
| `nvda.exe` | NVDA | `2313072301` | `-1981894995` |
| `nvda_nouiaccess.exe` | NVDA without UIAccess | `1304689049` | `1304689049` |
| `omencommandcenterbackground.exe` | HP OMEN Gaming Hub | `273289631` | `273289631` |
| `onenote.exe` | OneNote (Win32 desktop) | `1658786077` | `1658786077` |
| `opera.exe` | Opera and Opera GX | `3109566323` | `-1185400973` |
| `osk.exe` | On-Screen Keyboard | `1022247735` | `1022247735` |
| `pad.automationserver.exe` | Power Automate automation server | `1244711123` | `1244711123` |
| `pot.exe` | Pot App translator | `1545398617` | `1545398617` |
| `powerpnt.exe` | Microsoft PowerPoint | `3920376357` | `-374590939` |
| `powershell.exe` | Windows PowerShell 5.1 | `3623985111` | `-670982185` |
| `propresenter.exe` | ProPresenter | `1938163567` | `1938163567` |
| `prowritingaid.desktop.exe` | ProWritingAid Everywhere | `1150541663` | `1150541663` |
| `psr.exe` | Windows Steps Recorder | `4156128047` | `-138839249` |
| `pxitpapp.exe` | Unidentified | `732780018` | `732780018` |
| `python.exe` | Python | `3670683320` | `-624283976` |
| `pythonw.exe` | Python (no console) | `1394786065` | `1394786065` |
| `qianwen.exe` | Alibaba Qwen app | `57225726` | `57225726` |
| `qmaiservice64.exe` | Unidentified | `3039748689` | `-1255218607` |
| `qqpctray.exe` | Tencent PC Manager | `491213080` | `491213080` |
| `quark.exe` | Alibaba Quark browser | `2878172066` | `-1416795230` |
| `quicker.exe` | Quicker | `804916661` | `804916661` |
| `regsvcs.exe` | .NET Services Installation Tool | `2937490672` | `-1357476624` |
| `remotemouse.exe` | Remote Mouse | `2491548632` | `-1803418664` |
| `reverso.exe` | Reverso | `3215260459` | `-1079706837` |
| `rtkauduservice64.exe` | Realtek Audio Universal Service | `459851463` | `459851463` |
| `rundll32.exe` | Windows host process (Rundll32) | `854398208` | `854398208` |
| `sa.exe` | Unidentified | `1602550339` | `1602550339` |
| `sapisvr.exe` | Windows Speech Recognition | `1541843396` | `1541843396` |
| `shadowbot.uiautomation.provider.exe` | ShadowBot RPA | `1889756734` | `1889756734` |
| `sihost.exe` | Shell Infrastructure Host | `602801217` | `602801217` |
| `smartscreen.exe` | Microsoft Defender SmartScreen | `359265946` | `359265946` |
| `snagit32.exe` | TechSmith Snagit capture (legacy) | `2886728032` | `-1408239264` |
| `snagiteditor.exe` | TechSmith Snagit Editor | `1417251007` | `1417251007` |
| `snova.exe` | Dolphin SuperNova | `3637293433` | `-657673863` |
| `snowshot.exe` | Snow Shot | `2892721503` | `-1402245793` |
| `sogoucloud.exe` | Sogou Pinyin cloud input | `46599781` | `46599781` |
| `sogousmartassistant.exe` | Sogou Input AI Wangzai | `3530233603` | `-764733693` |
| `spa.exe` | Unidentified | `2690156078` | `-1604811218` |
| `spotify.exe` | Spotify | `1864496181` | `1864496181` |
| `startmenuexperiencehost.exe` | Start Menu Experience Host | `4274256911` | `-20710385` |
| `subst.exe` | Windows subst command | `3257941115` | `-1037026181` |
| `svchost.exe` | Windows Service Host | `3226547467` | `-1068419829` |
| `te.processhost.exe` | TAEF Process Host | `668962908` | `668962908` |
| `teamviewer_desktop.exe` | TeamViewer | `2716849167` | `-1578118129` |
| `temperatureindicator.exe` | Unidentified | `1089268748` | `1089268748` |
| `text blaze.exe` | Text Blaze | `1842752105` | `1842752105` |
| `textexpander.exe` | TextExpander | `1864328181` | `1864328181` |
| `textinputhost.exe` | Text Input Host | `4257023149` | `-37944147` |
| `timedoctor2.exe` | Time Doctor | `1821977218` | `1821977218` |
| `tracker.exe` | Unidentified | `2917047990` | `-1377919306` |
| `typeless.exe` | Typeless | `3472010722` | `-822956574` |
| `uihost.exe` | McAfee WebAdvisor | `3733597238` | `-561370058` |
| `uiprotectedbrowser.exe` | Trend Micro Titanium | `3495511117` | `-799456179` |
| `vivaldi.exe` | Vivaldi | `3822218420` | `-472748876` |
| `voiceaccess.exe` | Voice Access | `3295411586` | `-999555710` |
| `weixin.exe` | Tencent Weixin | `432524448` | `432524448` |
| `wermgr.exe` | Windows Problem Reporting | `800847719` | `800847719` |
| `winappdriver.exe` | Windows Application Driver | `791862251` | `791862251` |
| `wineyes.exe` | Window-Eyes | `621100454` | `621100454` |
| `winfocusmonitor.exe` | ControlUp for Desktops | `755368142` | `755368142` |
| `winword.exe` | Microsoft Word | `1588985658` | `1588985658` |
| `wispr flow helper.exe` | Wispr Flow Helper | `800252223` | `800252223` |
| `workpuls.exe` | Insightful (formerly Workpuls) | `3199174751` | `-1095792545` |
| `wps.exe` | WPS Office | `174440287` | `174440287` |
| `xlsmartutils.exe` | Unidentified | `664881751` | `664881751` |
| `youdaodict.exe` | Youdao Dictionary | `1093415826` | `1093415826` |
| `zero.exe` | Unidentified | `1343414199` | `1343414199` |
| `zoom.exe` | Zoom Workplace | `641500889` | `641500889` |
| `zt.exe` | ZoomText | `2323185757` | `-1971781539` |
